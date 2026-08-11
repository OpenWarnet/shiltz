#include "ItemConfirmNpc.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "enums/ItemConfirmFailReason.h"
#include "enums/ItemType.h"
#include "parser/ItemScr.h"
#include "protocol/client/ItemConfirmNpcRequest.h"
#include "protocol/server/ItemConfirmNpcFail.h"
#include "protocol/server/ItemConfirmNpcSucc.h"
#include "world/Player.h"
#include "world/World.h"

#include <iostream>
#include <random>

namespace
{
    // Not read from any table -- a flat requirement for every appraisable
    // item regardless of type.
    constexpr std::int32_t kLevelRequirementThreshold = 90;

    // A rolled result where every one of the 10 gates landed on baseline
    // tier 2 (i.e. nothing actually changed) is reported as option_bits=0
    // instead of the literal all-2s bit pattern -- two literals map to
    // this "nothing happened" state, differing only in an unused high bit.
    constexpr std::uint32_t kBaselinePattern1 = 0x12492492;
    constexpr std::uint32_t kBaselinePattern2 = 0x52492492;

    constexpr int kGateCount = 10;

    bool IsSlotInRange(std::uint32_t slot)
    {
        return slot <= 0x2F;
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

    // Rolls all 10 gates for one item instance. A gate only gets a fresh
    // roll if its bit is set in `eligibleMask` (this item instance's
    // "hidden roll" eligibility -- see PlayerItemSlot::option_eligible_mask);
    // otherwise it defaults to baseline tier 2 (no effect).
    std::uint32_t RollOptionBits(std::uint32_t eligibleMask, std::mt19937& rng)
    {
        std::uniform_int_distribution<int> dist(0, 999);

        std::uint32_t bits = 0;
        for (int gate = 0; gate < kGateCount; ++gate)
        {
            const int tier = (eligibleMask & (1u << gate)) ? BucketTier(dist(rng)) : 2;
            bits |= static_cast<std::uint32_t>(tier) << (gate * 3);
        }

        if (bits == kBaselinePattern1 || bits == kBaselinePattern2)
            bits = 0;

        return bits;
    }
}

void HandleItemConfirmNpcRequest(const GameContext& ctx, const ItemConfirmNpcRequest& request)
{
    std::cout << "Item confirm npc request: " << request.slot_ids.size() << " slot(s), npc_flag "
              << request.npc_flag << "\n";

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_ITEM_CONFIRM_NPC_REQUEST: socket has no resolved character "
                     "(never entered)\n";
        return;
    }

    std::random_device rd;
    std::mt19937 rng(rd());

    // One atomic pass: a per-slot gate failure just skips that slot and
    // the loop continues -- only the very end decides SUCC vs FAIL, based
    // on whether anything actually succeeded.
    std::vector<ItemConfirmNpcResult> results;
    std::int64_t runningFee = 0;

    for (std::uint32_t slotId : request.slot_ids)
    {
        if (!IsSlotInRange(slotId))
        {
            std::cout << "error inven_slot = " << slotId << "\n";
            continue;
        }

        auto content = session->player.LoadItemSlot(ctx.db, slotId);
        if (!content)
            continue; // empty slot -- nothing to appraise

        const ItemRecord* itemRecord = ctx.world.FindItemRecord(content->item_id);
        if (!itemRecord)
            continue; // unknown item_id -- no type/scale data to check against

        if (!IsItemTypeEligible(itemRecord->item_type))
        {
            std::cout << "not confirm item : item_type=" << itemRecord->item_type
                      << ", slot=" << slotId << "\n";
            continue;
        }

        // item_opt2 == -1 exempts the item from the level-90 gate.
        const bool exempt = content->item_opt2 == -1;
        if (!exempt && content->item_level < kLevelRequirementThreshold)
        {
            std::cout << "item_type=" << itemRecord->item_type << ", item_opt2=" << content->item_opt2
                      << ", item_level=" << content->item_level
                      << ", require_level=" << kLevelRequirementThreshold << "\n";
            continue;
        }

        const std::int64_t fee = itemRecord->buy_price / 10;
        if (runningFee + fee > session->player.money)
        {
            std::cout << "no have money : " << fee << ", (" << runningFee << ", "
                      << session->player.money << ")\n";
            continue;
        }

        runningFee += fee;

        content->option_bits = RollOptionBits(content->option_eligible_mask, rng);
        session->player.SaveItemSlot(ctx.db, slotId, *content);

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
        std::cout << "Sending GC_ITEM_CONFIRM_NPC_FAIL: result_code " << response.result_code << "\n";
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_ITEM_CONFIRM_NPC_FAIL, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
        return;
    }

    // Fee is deducted once for the whole request, not per slot.
    session->player.money -= runningFee;
    ctx.sessions.Set(ctx.clientSocket, *session);
    session->player.SaveMoney(ctx.db);

    ItemConfirmNpcSucc response{
        .results = results,
        .total_fee = static_cast<std::uint32_t>(runningFee),
    };
    std::cout << "Sending GC_ITEM_CONFIRM_NPC_SUCC: " << response.results.size()
              << " slot(s) appraised, total_fee " << response.total_fee << "\n";
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_ITEM_CONFIRM_NPC_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
}
