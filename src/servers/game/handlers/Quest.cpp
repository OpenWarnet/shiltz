#include "Quest.h"

#include "Outbox.h"
#include "Persistence.h"
#include "enums/ItemType.h"
#include "enums/QuestFailReason.h"
#include "parser/QuestScr.h"
#include "protocol/client/QuestResult.h"
#include "protocol/server/InventoryItemList.h"
#include "protocol/server/QuestFail.h"
#include "protocol/server/QuestSucc.h"
#include "protocol/server/ServerChange.h"
#include "repositories/CharacterRepository.h"
#include "repositories/ItemRepository.h"
#include "repositories/QuestFlagRepository.h"
#include "storage/Transaction.h"
#include "tables/GameData.h"
#include "tables/ItemTable.h"
#include "tables/WarpTable.h"
#include "world/Character.h"
#include "world/Player.h"
#include "world/World.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
// Total addressable bag slots (wire slots 0-12 are equipment, not bag) --
// same layout HandleItemPickup/HandleItemDrop key off.
constexpr std::uint32_t kBagSlotCount =
    static_cast<std::uint32_t>(InventoryItemList::kTotalSlots - InventoryItemList::kBagStartSlot);

std::int64_t CountItemInInventory(const Character& character, std::int64_t itemId)
{
    std::int64_t total = 0;
    for (const auto& entry : character.inventory)
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

// Checks every condition rather than stopping at the first failure.
bool ConditionsMet(const QuestConditions& c, const Character& character)
{
    bool met = true;

    if (c.has_item_0 != 0)
    {
        const std::int64_t have = CountItemInInventory(character, c.has_item_0);
        if (have < c.min_item_0_count)
            met = false;
    }

    if (c.has_item_1 != 0)
    {
        const std::int64_t have = CountItemInInventory(character, c.has_item_1);
        if (have < c.min_item_1_count)
            met = false;
    }

    if (c.has_flag != 0 && !character.quest_flags.IsSet(static_cast<std::uint32_t>(c.has_flag)))
        met = false;

    if (c.has_job != 0 && static_cast<std::uint32_t>(c.has_job) != character.job_id)
        met = false;

    // "Reputation" and "Fame" are the same Individuality System stat under
    // two names -- see reward_fame below and CharacterDataLoad.h's ownFame
    // comment.
    if (c.min_reputation != 0 && static_cast<std::int64_t>(character.fame) < c.min_reputation)
        met = false;

    if (c.min_level != 0 && static_cast<std::int64_t>(character.level) < c.min_level)
        met = false;

    if (c.min_cegel != 0 && character.money < c.min_cegel)
        met = false;

    // min_days/time_of_day: no "days played" counter or server clock exists
    // yet -- both are treated as always satisfied until that lands.

    return met;
}

std::optional<std::uint32_t> FindFreeBagSlot(const Character& character)
{
    for (std::uint32_t i = 0; i < kBagSlotCount; ++i)
    {
        if (!character.GetInventorySlot(i))
            return i;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> FindStackableBagSlot(const Character& character, std::uint32_t itemId)
{
    for (const auto& entry : character.inventory)
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

struct GrantedSlot
{
    std::uint32_t bag_index = 0;
    Item item;
};

// A committed turn-in: the reply plus what onDone applies to the live character.
struct QuestOutcome
{
    QuestSucc response;
    std::vector<GrantedSlot> slots;
    std::optional<std::int64_t> money;
    std::optional<std::int64_t> exp;
    std::optional<std::uint32_t> fame;
};

// Grants `count` (at least 1) units of itemId into the character's bag,
// writing through to the DB and the character as it goes. Returns the
// wire entries for GC_QUEST_SUCC; empty if itemId isn't a known item.scr
// row or the bag has no room left.
std::vector<QuestSuccItem> GrantRewardItem(IDatabase& db, const ItemTable& items, Character& character,
                                            std::vector<GrantedSlot>& slots, std::int64_t itemId,
                                            std::int64_t count)
{
    std::vector<QuestSuccItem> granted;

    if (itemId == 0)
        return granted;

    const ItemRecord* record = items.Find(itemId);
    if (!record)
        return granted;

    const auto wireItemId = static_cast<std::uint32_t>(itemId);
    const std::int64_t units = count > 0 ? count : 1;

    if (IsStackableItemType(record->item_type))
    {
        auto bagIndex = FindStackableBagSlot(character, wireItemId);
        std::optional<Item> existing;
        std::uint32_t existingQuantity = 0;
        if (bagIndex)
        {
            existing = character.GetInventorySlot(*bagIndex);
            existingQuantity = existing->quantity;
        }
        else
        {
            bagIndex = FindFreeBagSlot(character);
        }

        if (!bagIndex)
            return granted;

        Item stacked{
            .item_id = wireItemId,
            .quantity = existingQuantity + static_cast<std::uint32_t>(units),
            .has_refine_level = false,
        };

        // Guarded against this slot's own last-known content -- see
        // ItemRepository.h. A concurrently-changed slot just skips this
        // reward, same as "no free bag slot" above.
        if (!ItemRepository::SaveInventorySlot(db, character.id, *bagIndex, existing, stacked))
            return granted;
        character.SetInventorySlot(*bagIndex, stacked);
        slots.push_back(GrantedSlot{.bag_index = *bagIndex, .item = stacked});

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
        auto bagIndex = FindFreeBagSlot(character);
        if (!bagIndex)
            break;

        Item equipped{
            .item_id = wireItemId,
            .refine_level = 0,
            .has_refine_level = true,
            .option_bits = Item::kNeverAppraised,
        };

        // bagIndex just came from FindFreeBagSlot, so the expected previous
        // content is "empty" -- see ItemRepository.h.
        if (!ItemRepository::SaveInventorySlot(db, character.id, *bagIndex, std::nullopt, equipped))
            break;
        character.SetInventorySlot(*bagIndex, equipped);
        slots.push_back(GrantedSlot{.bag_index = *bagIndex, .item = equipped});

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

// Tells the client which game server/port to (re)connect to after a warp.
// Sent even though this warp doesn't actually move the character to a
// different physical server -- it's the packet the client expects
// following a location change, so it always names *this* game server.
// sessionId must be this connection's own login session id -- the
// client's reconnect CG_ENTER carries it back as-is, and Session.cpp's
// HandleEnter rejects anything that isn't a real `session` table row (see
// ServerChange.h).
void SendServerChange(const GameContext& ctx, std::uint32_t sessionId)
{
    // Same dev game-server IP/port GameHandover.cpp (login server) hands
    // the client via GameConnectSuccess -- there's no shared config between
    // the two processes, so this is kept in sync by hand; update both if
    // either changes. server_port is confirmed against real captures to
    // always be 1818, regardless of server_ip/channel_id (see
    // ServerChange.h).
    ServerChange response;
    response.server_ip = "45.58.9.172";
    response.session_id = sessionId;
    response.server_port = 1818;

    ctx.outbox.Send(ctx.connection, response);
}

// Runs the turn-in against a snapshot of the character in one transaction;
// nullopt if q.set_flag names a one-time quest already claimed -- see
// QuestFlagRepository::TryClaimFlag.
std::optional<QuestOutcome> ApplyConsequences(IDatabase& db, const ItemTable& items, Character character,
                                               const QuestConsequences& q, const WarpRecord* warp)
{
    const std::int64_t characterId = character.id;
    QuestOutcome outcome;

    DatabaseTransaction txn(db);

    // Claim the one-time-quest gate first, before granting anything -- see
    // QuestFlagRepository::TryClaimFlag. Quests with no set_flag (0) have no
    // such gate and always re-apply their consequences.
    if (q.set_flag != 0 && !QuestFlagRepository::TryClaimFlag(db, characterId, q.set_flag))
        return std::nullopt;

    std::vector<QuestSuccItem> granted;
    for (auto& item : GrantRewardItem(db, items, character, outcome.slots, q.reward_item_0, q.reward_item_0_count))
        granted.push_back(item);
    for (auto& item : GrantRewardItem(db, items, character, outcome.slots, q.reward_item_1, q.reward_item_1_count))
        granted.push_back(item);
    for (auto& item : GrantRewardItem(db, items, character, outcome.slots, q.reward_item_2, q.reward_item_2_count))
        granted.push_back(item);

    if (q.reward_cegel != 0)
    {
        outcome.money = CharacterRepository::AddMoney(db, characterId, q.reward_cegel);
        character.money = *outcome.money;
    }

    if (q.reward_exp != 0)
    {
        outcome.exp = CharacterRepository::AddExp(db, characterId, q.reward_exp);
        character.exp = *outcome.exp;
    }

    if (q.reward_fame != 0)
    {
        outcome.fame = CharacterRepository::AddFame(db, characterId, static_cast<std::uint32_t>(q.reward_fame));
        character.fame = *outcome.fame;
    }

    // Written with the rewards, so the reconnect after GC_SERVER_CHANGE loads the destination.
    if (warp)
    {
        character.map_id = static_cast<std::uint32_t>(warp->server_map_id);
        character.x = static_cast<std::int32_t>(warp->x);
        character.y = static_cast<std::int32_t>(warp->y);
        character.SavePosition(db);
    }

    txn.Commit();

    QuestSucc& response = outcome.response;
    response.items = std::move(granted);
    // set_flag is the only stable per-quest identifier this consequence
    // set carries -- 0 (no quest_id) if this node doesn't set one.
    response.quest_id = static_cast<std::uint32_t>(q.set_flag);
    response.money = static_cast<std::uint64_t>(character.money);
    response.fame = character.fame;
    response.exp = static_cast<std::uint64_t>(character.exp);
    response.ap = character.ap;
    response.hp = character.hp;
    return outcome;
}

void SendQuestFail(const GameContext& ctx)
{
    QuestFail response;
    response.result_code = static_cast<std::int32_t>(QuestFailReason::ConditionsNotMet);

    ctx.outbox.Send(ctx.connection, response);
}
} // namespace

void HandleQuestResult(const GameContext& ctx, const QuestResult& request, Player& player)
{
    const QuestActionRecord* record = ctx.data.quests.Find(static_cast<std::int64_t>(request.action_id));
    if (!record || !ConditionsMet(record->conditions, player.character))
        return SendQuestFail(ctx);

    const QuestConsequences& q = record->consequences;
    const WarpRecord* warp = q.warp_id != 0 ? ctx.data.warps.Find(q.warp_id) : nullptr;

    ctx.persistence.Run(
        [&items = ctx.data.items, character = player.character, &q, warp](IDatabase& db)
        { return ApplyConsequences(db, items, character, q, warp); },
        [ctx, &q, warp, sessionId = player.session_id](std::optional<QuestOutcome> outcome)
        {
            if (!outcome)
                return SendQuestFail(ctx);

            if (warp)
            {
                // Everything, destination included, is saved; the client reconnects with a fresh CG_ENTER.
                ctx.world.Leave(ctx.connection);
                SendServerChange(ctx, sessionId);
            }
            else if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                Character& character = player->character;
                if (q.set_flag != 0)
                    character.quest_flags.Set(static_cast<std::uint32_t>(q.set_flag), true);
                for (const GrantedSlot& slot : outcome->slots)
                    character.SetInventorySlot(slot.bag_index, slot.item);
                if (outcome->money)
                    character.money = *outcome->money;
                if (outcome->exp)
                    character.exp = *outcome->exp;
                if (outcome->fame)
                    character.fame = *outcome->fame;
            }

            ctx.outbox.Send(ctx.connection, outcome->response);
        },
        [ctx](const std::string&) { SendQuestFail(ctx); });
}
