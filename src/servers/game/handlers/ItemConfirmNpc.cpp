#include "ItemConfirmNpc.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "enums/ItemConfirmFailReason.h"
#include "enums/ItemType.h"
#include "parser/ItemScr.h"
#include "protocol/client/ItemConfirmNpcRequest.h"
#include "protocol/server/ItemConfirmNpcFail.h"
#include "protocol/server/ItemConfirmNpcSucc.h"
#include "repositories/ItemRepository.h"
#include "storage/Transaction.h"
#include "tables/GameData.h"
#include "tables/ItemTable.h"
#include "world/Item.h"
#include "world/Player.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <random>
#include <vector>

namespace
{
// Flat level requirement for rerolling an already-appraised item.
constexpr std::int32_t kLevelRequirementThreshold = 90;

// A rolled result where every one of the 10 gates landed on baseline
// tier 2 (i.e. nothing actually changed) is reported as option_bits=0
// instead of the literal all-2s bit pattern.
constexpr std::uint32_t kBaselinePattern1 = 0x12492492;
constexpr std::uint32_t kBaselinePattern2 = 0x52492492;

constexpr int kGateCount = 10;

// Malformed requests (empty or too many slots) are dropped silently
// rather than answered with a FAIL.
constexpr std::size_t kMaxSlotsPerRequest = 8;

bool IsSlotInRange(std::uint32_t slot)
{
    // Equipment slots (0-7) aren't appraisable via this path.
    return slot > 7 && slot <= 0x2F;
}

bool IsItemTypeEligible(std::int64_t rawItemType)
{
    switch (static_cast<ItemType>(rawItemType))
    {
    case ItemType::Material:
    case ItemType::Potion:
    case ItemType::Type2:
    case ItemType::Misc:
    case ItemType::PetEgg:
    case ItemType::Type23:
    case ItemType::Type24:
    case ItemType::Type27:
        return false;
    default:
        return true;
    }
}

// r is uniform over [0, 999]; each tier's share of that range is its
// roll probability:
//   tier 0 -> r in [  0, 109]  110/1000 = 11%
//   tier 1 -> r in [110, 259]  150/1000 = 15%
//   tier 2 -> r in [260, 459]  200/1000 = 20%
//   tier 3 -> r in [460, 709]  250/1000 = 25%
//   tier 4 -> r in [710, 939]  230/1000 = 23%
//   tier 5 -> r in [940, 969]   30/1000 =  3%
//   tier 6 -> r in [970, 989]   20/1000 =  2%
//   tier 7 -> r in [990, 999]   10/1000 =  1%
int BucketTier(int r)
{
    if (r <= 109)
        return 0;
    if (r <= 259)
        return 1;
    if (r <= 459)
        return 2;
    if (r <= 709)
        return 3;
    if (r <= 939)
        return 4;
    if (r <= 969)
        return 5;
    if (r <= 989)
        return 6;
    return 7;
}

// How many of an item's eligible gates activate on a single appraisal
// attempt -- a second, independent roll from BucketTier above:
//   0 gates -> 30.0%   1 gate  -> 34.0%   2 gates -> 20.0%
//   3 gates -> 10.0%   4 gates ->  3.0%   5-10 gates -> 0.5% each
int RollActivatedGateCount(std::mt19937& rng)
{
    std::uniform_int_distribution<int> dist(0, 999);
    const int r = dist(rng);
    if (r <= 299)
        return 0;
    if (r <= 639)
        return 1;
    if (r <= 839)
        return 2;
    if (r <= 939)
        return 3;
    if (r <= 969)
        return 4;
    if (r <= 974)
        return 5;
    if (r <= 979)
        return 6;
    if (r <= 984)
        return 7;
    if (r <= 989)
        return 8;
    if (r <= 994)
        return 9;
    return 10;
}

// Gate index -> the item's own per-gate scale, in the same order
// option_bits packs them. A gate is eligible to roll for an item if
// and only if its scale is nonzero.
std::array<std::int64_t, kGateCount> GateScales(const ItemRecord& item)
{
    return {
        item.damage_scale,       item.magic_power_scale,    item.defense_scale,
        item.attack_speed_scale, item.accuracy_scale,       item.critical_rate_scale,
        item.evasion_rate_scale, item.movement_speed_scale, item.hp_percent_scale,
        item.ap_percent_scale,
    };
}

std::vector<int> EligibleGates(const ItemRecord& item)
{
    const auto scales = GateScales(item);
    std::vector<int> eligible;
    for (int gate = 0; gate < kGateCount; ++gate)
    {
        if (scales[gate] != 0)
            eligible.push_back(gate);
    }
    return eligible;
}

// Rolls option_bits for one item instance. Every gate defaults to
// baseline tier 2 (no effect); only gates BOTH template-eligible
// (nonzero scale) AND drawn by this attempt's activation roll get a
// real BucketTier roll.
std::uint64_t RollOptionBits(const ItemRecord& item, std::mt19937& rng)
{
    const std::vector<int> eligible = EligibleGates(item);
    const int activateCount =
        std::min<int>(RollActivatedGateCount(rng), static_cast<int>(eligible.size()));

    std::vector<int> activated;
    std::sample(eligible.begin(), eligible.end(), std::back_inserter(activated), activateCount,
                rng);

    std::uniform_int_distribution<int> tierDist(0, 999);
    std::uint64_t bits = 0;
    for (int gate = 0; gate < kGateCount; ++gate)
    {
        const bool isActivated =
            std::find(activated.begin(), activated.end(), gate) != activated.end();
        const int tier = isActivated ? BucketTier(tierDist(rng)) : 2;
        bits |= static_cast<std::uint32_t>(tier) << (gate * 3);
    }

    if (bits == kBaselinePattern1 || bits == kBaselinePattern2)
        bits = 0;

    return bits;
}
} // namespace

void HandleItemConfirmNpcRequest(const GameContext& ctx, const ItemConfirmNpcRequest& request)
{
    std::cout << "Item confirm npc request: " << request.slot_ids.size() << " slot(s), npc_flag "
              << request.npc_flag << "\n";

    if (request.slot_ids.empty() || request.slot_ids.size() > kMaxSlotsPerRequest)
    {
        std::cout << "Dropping CG_ITEM_CONFIRM_NPC_REQUEST: count " << request.slot_ids.size()
                  << " out of range [1," << kMaxSlotsPerRequest << "]\n";
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_ITEM_CONFIRM_NPC_REQUEST: socket has no resolved character "
                     "(never entered)\n";
        return;
    }

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session are
    // copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session]() mutable {
    const int64_t characterId = session->characterId;

    std::random_device rd;
    std::mt19937 rng(rd());

    struct AppraisedSlot
    {
        std::uint32_t slot_id;
        Item item;
    };

    // One atomic pass: a per-slot gate failure just skips that slot and
    // the loop continues -- only the very end decides SUCC vs FAIL, based
    // on whether anything actually succeeded. Writes go to the DB inside
    // txn below; session->player and the session store only see them once
    // txn.Commit() has actually succeeded (see appraisedItems below), so a
    // write failure partway through can't leave the cache ahead of what's
    // really on disk.
    std::vector<ItemConfirmNpcResult> results;
    std::vector<AppraisedSlot> appraisedItems;
    std::int64_t runningFee = 0;

    DatabaseTransaction txn(ctx.db);

    for (std::uint32_t slotId : request.slot_ids)
    {
        if (!IsSlotInRange(slotId))
        {
            std::cout << "error inven_slot = " << slotId << "\n";
            continue;
        }

        auto content = session->player.GetItemSlot(slotId);
        if (!content)
            continue; // empty slot -- nothing to appraise

        const ItemRecord* itemRecord = ctx.data.items.Find(content->item_id);
        if (!itemRecord)
            continue; // unknown item_id -- no type/scale data to check against

        if (!IsItemTypeEligible(itemRecord->item_type))
        {
            std::cout << "not confirm item : item_type=" << itemRecord->item_type
                      << ", slot=" << slotId << "\n";
            continue;
        }

        // A never-appraised item always qualifies; an already-appraised
        // one can only be rerolled if its use-level requirement is under
        // the threshold.
        const bool neverAppraised = content->option_bits == Item::kNeverAppraised;
        if (itemRecord->min_level >= kLevelRequirementThreshold || !neverAppraised)
        {
            std::cout << "item_type=" << itemRecord->item_type
                      << ", option_bits=" << content->option_bits
                      << ", min_level=" << itemRecord->min_level
                      << ", require_level=" << kLevelRequirementThreshold << "\n";
            continue;
        }

        const std::int64_t fee = itemRecord->sell_price;
        if (runningFee + fee > session->player.money)
        {
            std::cout << "no have money : " << fee << ", (" << runningFee << ", "
                      << session->player.money << ")\n";
            continue;
        }

        runningFee += fee;

        content->option_bits = RollOptionBits(*itemRecord, rng);
        ItemRepository::SaveItemSlot(ctx.db, characterId, slotId, *content);
        appraisedItems.push_back(AppraisedSlot{.slot_id = slotId, .item = *content});

        results.push_back(ItemConfirmNpcResult{
            .slot_id = slotId,
            .option_bits = content->option_bits,
        });
    }

    PayloadWriter writer;

    if (results.empty())
    {
        ItemConfirmNpcFail response{
            .result_code = static_cast<std::int32_t>(ItemConfirmFailReason::NoSlotsAppraised),
        };
        std::cout << "Sending GC_ITEM_CONFIRM_NPC_FAIL: result_code " << response.result_code
                  << "\n";
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_ITEM_CONFIRM_NPC_FAIL, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
        return;
    }

    // Fee is deducted once for the whole request, not per slot. Written
    // inside the same transaction as the appraised slots above, so a
    // partway failure rolls back the fee along with them rather than
    // charging for appraisals that never landed.
    session->player.money -= runningFee;
    session->player.SaveMoney(ctx.db);

    txn.Commit();

    // Only mirror into the cache / session store once the transaction is
    // actually durable -- see the comment on appraisedItems above.
    for (const AppraisedSlot& appraised : appraisedItems)
        session->player.SetItemSlot(appraised.slot_id, appraised.item);
    ctx.sessions.Set(ctx.clientSocket, *session);

    ItemConfirmNpcSucc response{
        .results = results,
        .total_fee = static_cast<std::uint32_t>(runningFee),
    };
    std::cout << "Sending GC_ITEM_CONFIRM_NPC_SUCC: " << response.results.size()
              << " slot(s) appraised, total_fee " << response.total_fee << "\n";
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_ITEM_CONFIRM_NPC_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    });
}
