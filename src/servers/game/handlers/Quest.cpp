#include "Quest.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "enums/ItemType.h"
#include "enums/QuestFailReason.h"
#include "parser/QuestScr.h"
#include "protocol/client/QuestResult.h"
#include "protocol/server/InventoryItemList.h"
#include "protocol/server/QuestFail.h"
#include "protocol/server/QuestSucc.h"
#include "repositories/ItemRepository.h"
#include "repositories/QuestFlagRepository.h"
#include "storage/Transaction.h"
#include "tables/GameData.h"
#include "tables/ItemTable.h"
#include "world/Player.h"

#include <iostream>
#include <optional>
#include <vector>

namespace
{
// Total addressable bag slots (wire slots 0-12 are equipment, not bag) --
// same layout HandleItemPickup/HandleItemDrop key off.
constexpr std::uint32_t kBagSlotCount =
    static_cast<std::uint32_t>(InventoryItemList::kTotalSlots - InventoryItemList::kBagStartSlot);

std::int64_t CountItemInInventory(const Player& player, std::int64_t itemId)
{
    std::int64_t total = 0;
    for (const auto& entry : player.inventory)
    {
        if (entry.item.item_id == static_cast<std::uint32_t>(itemId))
            total += entry.item.has_refine_level ? 1 : entry.item.quantity;
    }
    return total;
}

// Same item_type categorization ItemConfirmNpc.cpp's IsItemTypeEligible
// uses to tell "material-like, stacks by quantity" items apart from
// equippable, one-per-slot gear -- see enums/ItemType.h.
bool IsStackableItemType(std::int64_t rawItemType)
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
        return true;
    default:
        return false;
    }
}

// Checks every condition rather than stopping at the first failure, and
// logs each one that fails with the actual-vs-required values -- so a
// rejected turn-in shows *why* in the log instead of just that it failed.
bool ConditionsMet(const QuestConditions& c, const Player& player)
{
    bool met = true;

    if (c.has_item_0 != 0)
    {
        const std::int64_t have = CountItemInInventory(player, c.has_item_0);
        if (have < c.min_item_0_count)
        {
            std::cout << "Quest condition not met: has_item_0 " << c.has_item_0 << " -- have "
                      << have << ", need " << c.min_item_0_count << "\n";
            met = false;
        }
    }

    if (c.has_item_1 != 0)
    {
        const std::int64_t have = CountItemInInventory(player, c.has_item_1);
        if (have < c.min_item_1_count)
        {
            std::cout << "Quest condition not met: has_item_1 " << c.has_item_1 << " -- have "
                      << have << ", need " << c.min_item_1_count << "\n";
            met = false;
        }
    }

    if (c.has_flag != 0 && !player.quest_flags.IsSet(static_cast<std::uint32_t>(c.has_flag)))
    {
        std::cout << "Quest condition not met: has_flag " << c.has_flag << " not set\n";
        met = false;
    }

    if (c.has_job != 0 && static_cast<std::uint32_t>(c.has_job) != player.job_id)
    {
        std::cout << "Quest condition not met: has_job " << c.has_job << " -- player job_id "
                  << player.job_id << "\n";
        met = false;
    }

    // "Reputation" and "Fame" are the same Individuality System stat under
    // two names -- see reward_fame below and CharacterDataLoad.h's ownFame
    // comment.
    if (c.min_reputation != 0 && static_cast<std::int64_t>(player.fame) < c.min_reputation)
    {
        std::cout << "Quest condition not met: min_reputation " << c.min_reputation
                  << " -- player fame " << player.fame << "\n";
        met = false;
    }

    if (c.min_level != 0 && static_cast<std::int64_t>(player.level) < c.min_level)
    {
        std::cout << "Quest condition not met: min_level " << c.min_level << " -- player level "
                  << player.level << "\n";
        met = false;
    }

    if (c.min_cegel != 0 && player.money < c.min_cegel)
    {
        std::cout << "Quest condition not met: min_cegel " << c.min_cegel << " -- player money "
                  << player.money << "\n";
        met = false;
    }

    // min_days/time_of_day: no "days played" counter or server clock exists
    // yet -- both are treated as always satisfied until that lands.

    return met;
}

std::optional<std::uint32_t> FindFreeBagSlot(const Player& player)
{
    for (std::uint32_t i = 0; i < kBagSlotCount; ++i)
    {
        if (!player.GetInventorySlot(i))
            return i;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> FindStackableBagSlot(const Player& player, std::uint32_t itemId)
{
    for (const auto& entry : player.inventory)
    {
        if (entry.item.item_id == itemId && !entry.item.has_refine_level)
            return entry.slot_index;
    }
    return std::nullopt;
}

std::uint32_t WireBagSlot(std::uint32_t bagIndex)
{
    return static_cast<std::uint32_t>(InventoryItemList::kBagStartSlot) + bagIndex;
}

// Grants `count` (at least 1) units of itemId into session's inventory,
// writing through to the DB and the session cache as it goes -- mirrors
// GrantQuestReward's pre-existing write-then-cache ordering. Returns the
// wire entries for GC_QUEST_SUCC; empty if itemId isn't a known item.scr
// row or the bag has no room left.
std::vector<QuestSuccItem> GrantRewardItem(const GameContext& ctx, GameSession& session,
                                            std::int64_t itemId, std::int64_t count)
{
    std::vector<QuestSuccItem> granted;

    if (itemId == 0)
        return granted;

    const ItemRecord* record = ctx.data.items.Find(itemId);
    if (!record)
    {
        std::cout << "Quest reward: unknown item_id " << itemId << " -- skipping\n";
        return granted;
    }

    const auto wireItemId = static_cast<std::uint32_t>(itemId);
    const std::int64_t units = count > 0 ? count : 1;
    const std::int64_t characterId = session.characterId;

    if (IsStackableItemType(record->item_type))
    {
        auto bagIndex = FindStackableBagSlot(session.player, wireItemId);
        std::uint32_t existingQuantity = 0;
        if (bagIndex)
        {
            existingQuantity = session.player.GetInventorySlot(*bagIndex)->quantity;
        }
        else
        {
            bagIndex = FindFreeBagSlot(session.player);
        }

        if (!bagIndex)
        {
            std::cout << "Quest reward: no free bag slot for item_id " << itemId << "\n";
            return granted;
        }

        Item stacked{
            .item_id = wireItemId,
            .quantity = existingQuantity + static_cast<std::uint32_t>(units),
            .has_refine_level = false,
        };

        ItemRepository::SaveInventorySlot(ctx.db, characterId, *bagIndex, stacked);
        session.player.SetInventorySlot(*bagIndex, stacked);

        granted.push_back(QuestSuccItem{
            .inventory_id = 1,
            .slot_id = WireBagSlot(*bagIndex),
            .item_id = wireItemId,
            .qty_or_refine = stacked.WireQuantityOrRefine(),
            .option = 0,
            .unknown2 = 0,
        });
        return granted;
    }

    // Equippable -- doesn't stack, so `units` lands in `units` separate
    // slots rather than one slot with quantity > 1.
    for (std::int64_t i = 0; i < units; ++i)
    {
        auto bagIndex = FindFreeBagSlot(session.player);
        if (!bagIndex)
        {
            std::cout << "Quest reward: no free bag slot for item_id " << itemId << " (granted "
                       << i << "/" << units << ")\n";
            break;
        }

        Item equipped{
            .item_id = wireItemId,
            .refine_level = 0,
            .has_refine_level = true,
            .option_bits = Item::kNeverAppraised,
        };

        ItemRepository::SaveInventorySlot(ctx.db, characterId, *bagIndex, equipped);
        session.player.SetInventorySlot(*bagIndex, equipped);

        granted.push_back(QuestSuccItem{
            .inventory_id = 1,
            .slot_id = WireBagSlot(*bagIndex),
            .item_id = wireItemId,
            .qty_or_refine = equipped.WireQuantityOrRefine(),
            .option = equipped.option_bits,
            .unknown2 = 0,
        });
    }

    return granted;
}

void LogUnhandledConsequences(const QuestConsequences& q)
{
    // These need their own server-side mechanism/packet (map transfer,
    // job-change confirmation, skill grant, revival-point registration) --
    // not something GC_QUEST_SUCC alone can express. Parsed so the data is
    // available once that lands, but not applied yet.
    if (q.teleport_map_id != 0)
        std::cout << "Quest consequence not yet handled: teleport_map_id " << q.teleport_map_id << "\n";
    if (q.change_job_id != 0)
        std::cout << "Quest consequence not yet handled: change_job_id " << q.change_job_id << "\n";
    if (q.add_skill_ids != 0)
        std::cout << "Quest consequence not yet handled: add_skill_ids " << q.add_skill_ids << "\n";
    if (q.revival_point_id != 0)
        std::cout << "Quest consequence not yet handled: revival_point_id " << q.revival_point_id
                  << "\n";
}

QuestSucc ApplyConsequences(const GameContext& ctx, GameSession& session, const QuestConsequences& q)
{
    Player& player = session.player;
    const std::int64_t characterId = session.characterId;

    DatabaseTransaction txn(ctx.db);

    std::vector<QuestSuccItem> items;
    for (auto& item : GrantRewardItem(ctx, session, q.reward_item_0, q.reward_item_0_count))
        items.push_back(item);
    for (auto& item : GrantRewardItem(ctx, session, q.reward_item_1, q.reward_item_1_count))
        items.push_back(item);
    for (auto& item : GrantRewardItem(ctx, session, q.reward_item_2, q.reward_item_2_count))
        items.push_back(item);

    if (q.set_flag != 0)
    {
        QuestFlagRepository::SetFlag(ctx.db, characterId, q.set_flag);
        player.quest_flags.Set(static_cast<std::uint32_t>(q.set_flag), true);
    }

    if (q.reward_cegel != 0)
    {
        player.money += q.reward_cegel;
        player.SaveMoney(ctx.db);
    }

    if (q.reward_exp != 0)
    {
        player.exp += q.reward_exp;
        player.SaveLevel(ctx.db); // exp only here -- level is untouched
    }

    if (q.reward_fame != 0)
    {
        player.fame += static_cast<std::uint32_t>(q.reward_fame);
        player.SaveFame(ctx.db);
    }

    txn.Commit();

    LogUnhandledConsequences(q);

    return QuestSucc{
        .items = std::move(items),
        // set_flag is the only stable per-quest identifier this
        // consequence set carries -- 0 (no quest_id) if this node doesn't
        // set one.
        .quest_id = static_cast<std::uint32_t>(q.set_flag),
        .money = static_cast<std::uint64_t>(player.money),
        .fame = player.fame,
        .exp = static_cast<std::uint64_t>(player.exp),
        .ap = player.ap,
        .hp = player.hp,
    };
}

void SendQuestFail(const GameContext& ctx)
{
    QuestFail response{.result_code = static_cast<std::int32_t>(QuestFailReason::ConditionsNotMet)};
    std::cout << "Sending GC_QUEST_FAIL: result_code " << response.result_code << "\n";

    PayloadWriter writer;
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_QUEST_FAIL, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
}
} // namespace

void HandleQuestResult(const GameContext& ctx, const QuestResult& request)
{
    std::cout << "Quest result: action_id " << request.action_id << ", creature_instance_id "
              << request.creature_instance_id << " unknown: " << request.unknown << "\n";

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout
            << "Rejecting CG_QUEST_RESULT: socket has no resolved character (never entered)\n";
        return;
    }

    const QuestActionRecord* record =
        ctx.data.quests.Find(static_cast<std::int64_t>(request.action_id));
    if (!record)
    {
        std::cout << "Quest action_id " << request.action_id << " not found in quest.scr\n";
        SendQuestFail(ctx);
        return;
    }

    if (!ConditionsMet(record->conditions, session->player))
    {
        std::cout << "Quest action_id " << request.action_id << " conditions not met\n";
        SendQuestFail(ctx);
        return;
    }

    QuestSucc response = ApplyConsequences(ctx, *session, record->consequences);
    ctx.sessions.Set(ctx.clientSocket, *session);

    std::cout << "Sending GC_QUEST_SUCC: " << response.items.size() << " item(s), money "
              << response.money << ", fame " << response.fame << ", exp " << response.exp
              << ", ap " << response.ap << ", hp " << response.hp << "\n";

    PayloadWriter writer;
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_QUEST_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
}
