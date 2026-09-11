#include "ItemConfirmNpc.h"

#include "Outbox.h"
#include "Persistence.h"
#include "enums/ItemConfirmFailReason.h"
#include "enums/ItemType.h"
#include "parser/ItemScr.h"
#include "protocol/client/ItemConfirmNpcRequest.h"
#include "protocol/server/ItemConfirmNpcFail.h"
#include "protocol/server/ItemConfirmNpcSucc.h"
#include "repositories/CharacterRepository.h"
#include "repositories/ItemRepository.h"
#include "storage/Transaction.h"
#include "tables/GameData.h"
#include "tables/ItemTable.h"
#include "world/Item.h"
#include "world/Player.h"
#include "world/World.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <random>
#include <string>
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

void HandleItemConfirmNpcRequest(const GameContext& ctx, const ItemConfirmNpcRequest& request, Player& player)
{
    if (request.slot_ids.empty() || request.slot_ids.size() > kMaxSlotsPerRequest)
        return;

    auto sendFail = [ctx]
    {
        ItemConfirmNpcFail response;
        response.result_code = static_cast<std::int32_t>(ItemConfirmFailReason::NoSlotsAppraised);
        ctx.outbox.Send(ctx.connection, response);
    };

    struct Candidate
    {
        std::uint32_t slot_id;
        Item original;
        Item appraised;
        std::int64_t fee;
    };

    std::random_device rd;
    std::mt19937 rng(rd());

    // Rolled up front; the DB job decides which of them land.
    std::vector<Candidate> candidates;
    for (std::uint32_t slotId : request.slot_ids)
    {
        if (!IsSlotInRange(slotId))
            continue;

        auto content = player.character.GetItemSlot(slotId);
        if (!content)
            continue;

        const ItemRecord* itemRecord = ctx.data.items.Find(content->item_id);
        if (!itemRecord || !IsItemTypeEligible(itemRecord->item_type))
            continue;

        const bool neverAppraised = content->option_bits == Item::kNeverAppraised;
        if (itemRecord->min_level >= kLevelRequirementThreshold || !neverAppraised)
            continue;

        Item appraised = *content;
        appraised.option_bits = RollOptionBits(*itemRecord, rng);
        candidates.push_back(Candidate{
            .slot_id = slotId, .original = *content, .appraised = appraised, .fee = itemRecord->sell_price});
    }

    struct Appraisal
    {
        std::vector<Candidate> landed;
        std::int64_t fee = 0;
        std::int64_t money = 0;
    };

    const std::int64_t characterId = player.character.id;
    const std::int64_t money = player.character.money;

    // A slot that changed underneath or no longer fits the money is skipped, not charged; one debit for the rest.
    auto writes = [=](IDatabase& db) -> std::optional<Appraisal>
    {
        DatabaseTransaction txn(db);
        Appraisal done;
        for (const Candidate& candidate : candidates)
        {
            if (done.fee + candidate.fee > money)
                continue;
            if (!ItemRepository::SaveItemSlot(db, characterId, candidate.slot_id, candidate.original,
                                              candidate.appraised))
                continue;
            done.fee += candidate.fee;
            done.landed.push_back(candidate);
        }
        if (done.landed.empty())
            return std::nullopt;

        auto newMoney = CharacterRepository::TrySpendMoney(db, characterId, done.fee);
        if (!newMoney)
            return std::nullopt;
        txn.Commit();
        done.money = *newMoney;
        return done;
    };

    ctx.persistence.Run(
        writes,
        [ctx, sendFail](std::optional<Appraisal> done)
        {
            if (!done)
                return sendFail();

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                player->character.money = done->money;
                for (const Candidate& candidate : done->landed)
                    player->character.SetItemSlot(candidate.slot_id, candidate.appraised);
            }

            ItemConfirmNpcSucc response;
            for (const Candidate& candidate : done->landed)
                response.results.push_back(ItemConfirmNpcResult{
                    .slot_id = candidate.slot_id,
                    .option_bits = candidate.appraised.option_bits,
                });
            response.total_fee = static_cast<std::uint32_t>(done->fee);
            ctx.outbox.Send(ctx.connection, response);
        },
        [sendFail](const std::string&) { sendFail(); });
}
