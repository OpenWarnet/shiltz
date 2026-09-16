#include "Inventory.h"

#include "Outbox.h"
#include "Persistence.h"
#include "protocol/client/ItemDelete.h"
#include "protocol/client/ItemDrop.h"
#include "protocol/client/ItemMove.h"
#include "protocol/client/ItemPickup.h"
#include "protocol/server/InventoryItemList.h"
#include "protocol/server/ItemDeleteSuccess.h"
#include "protocol/server/ItemDropSuccess.h"
#include "protocol/server/ItemMoveFail.h"
#include "protocol/server/ItemMoveSuccess.h"
#include "protocol/server/ItemPickupSuccess.h"
#include "repositories/ItemRepository.h"
#include "storage/Transaction.h"
#include "world/Drop.h"
#include "world/Map.h"
#include "world/Player.h"
#include "world/World.h"
#include "world/common/EntityIdGenerator.h"

#include <algorithm>
#include <optional>
#include <string>

namespace
{
// Wire slot ids count bag slots from InventoryItemList::kBagStartSlot; inventory_slot is 0-based.
std::uint32_t BagIndex(std::uint32_t wireSlotId)
{
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(wireSlotId) -
                                      static_cast<std::int64_t>(InventoryItemList::kBagStartSlot));
}

// Runs the guarded writes in one transaction; false if any slot changed underneath (see ItemRepository.h).
template <typename Writes> auto Transactionally(Writes writes)
{
    return [writes](IDatabase& db)
    {
        DatabaseTransaction txn(db);
        if (!writes(db))
            return false;
        txn.Commit();
        return true;
    };
}
} // namespace

void HandleItemPickup(const GameContext& ctx, const ItemPickup& request, Player& player)
{
    Map* map = ctx.world.GetMap(player.character.map_id);
    if (!map)
        return;

    // Claimed before the DB write, so two players racing the same item can't both get it.
    auto drop = map->Despawn(Drop{.id = request.id});
    if (!drop)
        return;

    const std::int64_t characterId = player.character.id;
    const std::uint32_t slotIndex = BagIndex(request.slot_id);
    const std::uint32_t itemId = drop->item.item_id;
    const std::optional<Item> existing = player.character.GetInventorySlot(slotIndex);

    // A slot holding an incompatible item means the client's view is stale: put it back.
    const std::optional<Item> resolved = drop->item.StackedInto(existing, drop->item.quantity);
    if (!resolved)
        return map->Spawn(*drop);

    const Item updated = *resolved;

    auto putBack = [map, drop = *drop] { map->Spawn(drop); };

    ctx.persistence.Run(
        Transactionally([=](IDatabase& db)
                        { return ItemRepository::SaveInventorySlot(db, characterId, slotIndex, existing, updated); }),
        [ctx, request, slotIndex, itemId, updated, putBack](bool saved)
        {
            if (!saved)
                return putBack();

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                player->character.SetInventorySlot(slotIndex, updated);
            }

            ItemPickupSuccess succResponse;
            succResponse.id = request.id;
            succResponse.slot_id = request.slot_id;
            succResponse.item_id = itemId;
            succResponse.qty_or_refine = updated.WireQuantityOrRefine();
            ctx.outbox.Send(ctx.connection, succResponse);
        },
        [putBack](const std::string&) { putBack(); });
}

void HandleItemMove(const GameContext& ctx, const ItemMove& request, Player& player)
{
    auto sendFail = [ctx, request]
    {
        ItemMoveFail response;
        response.source_slot_id = request.source_slot_id;
        ctx.outbox.Send(ctx.connection, response);
    };

    const std::int64_t characterId = player.character.id;
    const std::optional<Item> sourceContent = player.character.GetItemSlot(request.source_slot_id);
    const std::optional<Item> destContent = player.character.GetItemSlot(request.dest_slot_id);
    if (!sourceContent && !destContent)
        return sendFail();

    const std::uint32_t source = request.source_slot_id;
    const std::uint32_t dest = request.dest_slot_id;

    // Both slots change together, or a crash would leave the item in both.
    auto writes = [=](IDatabase& db)
    {
        if (sourceContent && destContent)
            return ItemRepository::SaveItemSlot(db, characterId, source, sourceContent, *destContent) &&
                   ItemRepository::SaveItemSlot(db, characterId, dest, destContent, *sourceContent);
        if (sourceContent)
            return ItemRepository::SaveItemSlot(db, characterId, dest, destContent, *sourceContent) &&
                   ItemRepository::ClearItemSlot(db, characterId, source, sourceContent);
        return ItemRepository::SaveItemSlot(db, characterId, source, sourceContent, *destContent) &&
               ItemRepository::ClearItemSlot(db, characterId, dest, destContent);
    };

    ctx.persistence.Run(
        Transactionally(writes),
        [ctx, request, source, dest, sourceContent, destContent, sendFail](bool saved)
        {
            if (!saved)
                return sendFail();

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                Character& character = player->character;
                if (sourceContent && destContent)
                {
                    character.SetItemSlot(source, *destContent);
                    character.SetItemSlot(dest, *sourceContent);
                }
                else if (sourceContent)
                {
                    character.SetItemSlot(dest, *sourceContent);
                    character.ClearItemSlot(source);
                }
                else
                {
                    character.SetItemSlot(source, *destContent);
                    character.ClearItemSlot(dest);
                }
            }

            ItemMoveSuccess response;
            response.source_slot_id = request.source_slot_id;
            response.dest_slot_id = request.dest_slot_id;
            ctx.outbox.Send(ctx.connection, response);
        },
        [sendFail](const std::string&) { sendFail(); });
}

void HandleItemDrop(const GameContext& ctx, const ItemDrop& request, Player& player)
{
    if (request.quantity == 0)
        return;

    Map* map = ctx.world.GetMap(player.character.map_id);
    if (!map)
        return;

    const std::int64_t characterId = player.character.id;
    const std::uint32_t slotIndex = BagIndex(request.slot_id);
    const std::optional<Item> slotContent = player.character.GetInventorySlot(slotIndex);
    if (!slotContent)
        return;

    const std::uint32_t itemId = slotContent->item_id;

    // The ground item keeps item_level/option_bits so a later pickup gets them back.
    Item droppedItem = *slotContent;
    Item remainingItem;
    std::uint32_t remainingQuantity = 0;
    bool slotCleared = true;

    if (!slotContent->has_refine_level)
    {
        const std::uint32_t currentQuantity = slotContent->quantity;
        const std::uint32_t toDrop = std::min<std::uint32_t>(request.quantity, currentQuantity);
        droppedItem.quantity = toDrop;
        slotCleared = toDrop >= currentQuantity;
        if (!slotCleared)
        {
            remainingQuantity = currentQuantity - toDrop;
            remainingItem = *slotContent;
            remainingItem.quantity = remainingQuantity;
        }
    }
    else
    {
        // Equippable items always drop whole.
        droppedItem.quantity = 1;
    }

    // Dropped at the character's live position, not the stale DB row.
    const std::uint32_t dropX = player.character.placement.x;
    const std::uint32_t dropY = player.character.placement.y;

    auto writes = [=](IDatabase& db)
    {
        return slotCleared
                   ? ItemRepository::ClearInventorySlot(db, characterId, slotIndex, slotContent)
                   : ItemRepository::SaveInventorySlot(db, characterId, slotIndex, slotContent, remainingItem);
    };

    ctx.persistence.Run(
        Transactionally(writes),
        [=](bool saved)
        {
            if (!saved)
                return;

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                if (slotCleared)
                    player->character.ClearInventorySlot(slotIndex);
                else
                    player->character.SetInventorySlot(slotIndex, remainingItem);
            }

            // Created only once the slot removal is durable, so a crash can't duplicate the item.
            const std::uint32_t groundId = EntityIdGenerator::Next();
            map->Spawn(Drop{.id = groundId, .x = dropX, .y = dropY, .item = droppedItem});

            ItemDropSuccess succResponse;
            succResponse.id = groundId;
            succResponse.x = dropX;
            succResponse.y = dropY;
            succResponse.item_id = itemId;
            succResponse.source_slot_id = request.slot_id;
            succResponse.new_item_id = remainingQuantity > 0 ? itemId : 0;
            succResponse.new_item_count = remainingQuantity > 0 ? remainingQuantity - 1 : 0;
            ctx.outbox.Send(ctx.connection, succResponse);
        },
        [](const std::string&) {});
}

void HandleItemDelete(const GameContext& ctx, const ItemDelete& request, Player& player)
{
    // slot_id is a wire slot; ItemRepository resolves the equipment/inventory split itself.
    const std::int64_t characterId = player.character.id;
    const std::uint32_t slotId = request.slot_id;
    const std::optional<Item> slotContent = player.character.GetItemSlot(slotId);
    if (!slotContent)
        return;

    ctx.persistence.Run(
        Transactionally([=](IDatabase& db)
                        { return ItemRepository::ClearItemSlot(db, characterId, slotId, slotContent); }),
        [ctx, slotId](bool saved)
        {
            if (!saved)
                return;

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                player->character.ClearItemSlot(slotId);
            }

            ItemDeleteSuccess succResponse;
            succResponse.slot_id = slotId;
            ctx.outbox.Send(ctx.connection, succResponse);
        },
        [](const std::string&) {});
}
