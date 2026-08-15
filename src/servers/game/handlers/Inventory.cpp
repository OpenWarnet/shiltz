#include "Inventory.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/ItemDelete.h"
#include "protocol/client/ItemDrop.h"
#include "protocol/client/ItemMove.h"
#include "protocol/client/ItemPickup.h"
#include "protocol/server/InventoryItemList.h"
#include "protocol/server/ItemDeleteSuccess.h"
#include "protocol/server/ItemDropSuccess.h"
#include "protocol/server/ItemMapNew.h"
#include "protocol/server/ItemMapRemove.h"
#include "protocol/server/ItemMoveFail.h"
#include "protocol/server/ItemMoveSuccess.h"
#include "protocol/server/ItemPickupSuccess.h"
#include "repositories/ItemRepository.h"
#include "storage/Transaction.h"
#include "world/GroundItem.h"
#include "world/World.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <random>

void HandleItemPickup(const GameContext& ctx, const ItemPickup& request)
{
    std::cout << "Item pickup: ground id " << request.id << ", slot_id " << request.slot_id << "\n";

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_ITEM_PICKUP: socket has no resolved character (never entered)\n";
        return;
    }

    const int64_t characterId = session->characterId;
    // Client-side slot_id is wire-relative (bag slots start at
    // InventoryItemList::kBagStartSlot, per GC_INVENTORY_ITEM_LIST's
    // layout), but inventory_slot.slot_index is bag-relative starting at 0
    // -- convert before touching the DB. slot_id < kBagStartSlot (an
    // equipment slot, which this table doesn't cover) lands on a wrapped
    // uint32_t that will simply never match a real row.
    const auto slotIndex = static_cast<std::uint32_t>(static_cast<int64_t>(request.slot_id) -
                                                        static_cast<int64_t>(InventoryItemList::kBagStartSlot));

    Map* map = ctx.world.GetMap(session->player.map_id);
    if (!map)
    {
        std::cout << "Rejecting CG_ITEM_PICKUP: character on unknown map_id "
                  << session->player.map_id << "\n";
        return;
    }

    // Atomically claim the ground item so two players racing the same
    // pickup can't both grant it to themselves. Claimed before the DB
    // write, so a failed write loses the item rather than duplicating it.
    auto groundItem = map->TryTakeItem(request.id);
    if (!groundItem)
    {
        std::cout << "Rejecting CG_ITEM_PICKUP: no ground item with id " << request.id << "\n";
        return;
    }

    const std::uint32_t itemId = groundItem->item.item_id;

    DatabaseTransaction txn(ctx.db);

    // Trust the client-given slot rather than computing one server-side --
    // but verify it's actually consistent with the item being picked up:
    // empty is fine (new stack), holding the same item_id is fine (stack
    // onto it), holding a *different* item_id means the client's view of
    // its own inventory is stale/wrong, so ignore the request rather than
    // clobbering whatever's actually there.
    auto existing = session->player.GetInventorySlot(slotIndex);

    Item updated;
    if (existing)
    {
        if (existing->item_id != itemId)
        {
            std::cout << "Rejecting CG_ITEM_PICKUP: slot_index " << slotIndex << " holds item_id "
                      << existing->item_id << ", not " << itemId << " -- ignoring\n";
            // Put the claimed item back rather than dropping it -- this is
            // a normal rejection (stale client state), not a failure.
            map->AddItem(*groundItem);
            return;
        }

        // Stack onto the existing slot -- item_level/option_bits stay the
        // existing stack's (a stackable item's option_bits/item_level
        // aren't meaningful per-unit), only quantity moves.
        updated = *existing;
        updated.quantity += 1;

        std::cout << "Stacked item_id " << itemId << " at slot_index " << slotIndex << " (qty now "
                  << updated.quantity << ")\n";
    }
    else
    {
        // New slot -- adopt the ground item's full Item payload (so a
        // dropped item's item_level/option_bits survive the round-trip),
        // but pickup always claims one unit regardless of how many the
        // ground stack held.
        updated = groundItem->item;
        updated.quantity = 1;

        std::cout << "Added item_id " << itemId << " at slot_index " << slotIndex << "\n";
    }

    ItemRepository::SaveInventorySlot(ctx.db, characterId, slotIndex, updated);

    txn.Commit();

    session->player.SetInventorySlot(slotIndex, updated);
    ctx.sessions.Set(ctx.clientSocket, *session);

    PayloadWriter succWriter;
    ItemPickupSuccess succResponse{
        .id = request.id,
        .slot_id = request.slot_id,
        .item_id = itemId,
        .qty_or_refine = updated.WireQuantityOrRefine(),
    };
    succResponse.Serialize(succWriter);
    auto succData = succWriter.Data();

    GamePacket succPacket(GameOpcode::GC_ITEM_PICKUP_SUCC, succData);
    auto succPayload = succPacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, succPayload);

    PayloadWriter removeWriter;
    ItemMapRemove removeResponse{.id = request.id};
    removeResponse.Serialize(removeWriter);
    auto removeData = removeWriter.Data();

    GamePacket removePacket(GameOpcode::GC_ITEM_MAP_REMOVE, removeData);
    auto removePayload = removePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, removePayload);
}

void HandleItemMove(const GameContext& ctx, const ItemMove& request)
{
    std::cout << "Item move: source_slot_id " << request.source_slot_id << ", dest_slot_id "
              << request.dest_slot_id << "\n";

    auto sendFail = [&]
    {
        PayloadWriter failWriter;
        ItemMoveFail{.source_slot_id = request.source_slot_id}.Serialize(failWriter);
        auto failData = failWriter.Data();

        GamePacket failPacket(GameOpcode::GC_ITEM_MOVE_FAIL, failData);
        auto failPayload = failPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, failPayload);
    };

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_ITEM_MOVE: socket has no resolved character (never entered)\n";
        sendFail();
        return;
    }

    const int64_t characterId = session->characterId;

    // The two writes below must be all-or-nothing, or a crash between them
    // leaves the item's old and new slots both occupied (duplication).
    DatabaseTransaction txn(ctx.db);

    auto sourceContent = session->player.GetItemSlot(request.source_slot_id);
    auto destContent = session->player.GetItemSlot(request.dest_slot_id);

    if (!sourceContent && !destContent)
    {
        std::cout << "Rejecting CG_ITEM_MOVE: source_slot_id " << request.source_slot_id
                  << " and dest_slot_id " << request.dest_slot_id << " are both empty\n";
        sendFail();
        return;
    }

    if (sourceContent && destContent)
    {
        // Both occupied -- swap contents. Can't swap by relocating a
        // primary key across tables the way same-table moves used to, so
        // this always writes full content both ways instead.
        ItemRepository::SaveItemSlot(ctx.db, characterId, request.source_slot_id, *destContent);
        ItemRepository::SaveItemSlot(ctx.db, characterId, request.dest_slot_id, *sourceContent);

        session->player.SetItemSlot(request.source_slot_id, *destContent);
        session->player.SetItemSlot(request.dest_slot_id, *sourceContent);

        std::cout << "Swapped source_slot_id " << request.source_slot_id << " and dest_slot_id "
                  << request.dest_slot_id << "\n";
    }
    else if (sourceContent)
    {
        // dest is empty -- move source's content there and clear source.
        ItemRepository::SaveItemSlot(ctx.db, characterId, request.dest_slot_id, *sourceContent);
        ItemRepository::ClearItemSlot(ctx.db, characterId, request.source_slot_id);

        session->player.SetItemSlot(request.dest_slot_id, *sourceContent);
        session->player.ClearItemSlot(request.source_slot_id);

        std::cout << "Moved source_slot_id " << request.source_slot_id << " to empty dest_slot_id "
                  << request.dest_slot_id << "\n";
    }
    else
    {
        // source is empty, dest occupied -- move the other way.
        ItemRepository::SaveItemSlot(ctx.db, characterId, request.source_slot_id, *destContent);
        ItemRepository::ClearItemSlot(ctx.db, characterId, request.dest_slot_id);

        session->player.SetItemSlot(request.source_slot_id, *destContent);
        session->player.ClearItemSlot(request.dest_slot_id);

        std::cout << "Moved dest_slot_id " << request.dest_slot_id << " to empty source_slot_id "
                  << request.source_slot_id << "\n";
    }

    txn.Commit();

    ctx.sessions.Set(ctx.clientSocket, *session);

    PayloadWriter writer;
    ItemMoveSuccess response{
        .source_slot_id = request.source_slot_id,
        .dest_slot_id = request.dest_slot_id,
    };
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket responsePacket(GameOpcode::CG_ITEM_MOVE_SUCC, data);
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);
}

void HandleItemDrop(const GameContext& ctx, const ItemDrop& request)
{
    std::cout << "Item drop: slot_id " << request.slot_id << ", quantity " << request.quantity << "\n";

    if (request.quantity == 0)
    {
        std::cout << "Rejecting CG_ITEM_DROP: quantity 0\n";
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_ITEM_DROP: socket has no resolved character (never entered)\n";
        return;
    }

    Map* map = ctx.world.GetMap(session->player.map_id);
    if (!map)
    {
        std::cout << "Rejecting CG_ITEM_DROP: character on unknown map_id "
                  << session->player.map_id << "\n";
        return;
    }

    const int64_t characterId = session->characterId;
    // Same wire-relative -> bag-relative conversion as HandleItemPickup --
    // see the comment there.
    const auto slotIndex = static_cast<std::uint32_t>(static_cast<int64_t>(request.slot_id) -
                                                        static_cast<int64_t>(InventoryItemList::kBagStartSlot));

    // The slot removal must commit before the ground item is created --
    // otherwise a crash between the two loses or duplicates the item.
    DatabaseTransaction txn(ctx.db);

    auto slotContent = session->player.GetInventorySlot(slotIndex);
    if (!slotContent)
    {
        std::cout << "Rejecting CG_ITEM_DROP: slot_index " << slotIndex << " is empty\n";
        return;
    }

    const std::uint32_t itemId = slotContent->item_id;

    // Carries item_level/option_bits onto the ground item so they survive
    // to a subsequent pickup -- see world/GroundItem.h.
    Item droppedItem = *slotContent;
    // What's left in slotIndex after the drop -- 0 means the slot ended up
    // empty (fully dropped, or an equippable item, which always drops
    // whole).
    std::uint32_t remainingQuantity = 0;

    if (!slotContent->has_refine_level)
    {
        const std::uint32_t currentQuantity = slotContent->quantity;
        // Explicit template argument (not just std::min(...)) dodges the
        // Windows.h min/max macro collision -- see HexDump.h for the same
        // idiom; this project doesn't define NOMINMAX anywhere.
        const std::uint32_t toDrop = std::min<std::uint32_t>(request.quantity, currentQuantity);
        droppedItem.quantity = toDrop;

        if (toDrop >= currentQuantity)
        {
            ItemRepository::ClearInventorySlot(ctx.db, characterId, slotIndex);
            session->player.ClearInventorySlot(slotIndex);
        }
        else
        {
            remainingQuantity = currentQuantity - toDrop;

            Item remaining = *slotContent;
            remaining.quantity = remainingQuantity;
            ItemRepository::SaveInventorySlot(ctx.db, characterId, slotIndex, remaining);
            session->player.SetInventorySlot(slotIndex, remaining);
        }
    }
    else
    {
        // Equippable-style item (refine_level, not stackable) -- dropping
        // always removes the whole thing, regardless of requested quantity.
        droppedItem.quantity = 1;
        ItemRepository::ClearInventorySlot(ctx.db, characterId, slotIndex);
        session->player.ClearInventorySlot(slotIndex);
    }

    txn.Commit();

    ctx.sessions.Set(ctx.clientSocket, *session);

    // Player::x/y is kept live by HandleMovement on every CG_MOVE, unlike
    // character_position (only written at creation) -- drop at the
    // session's actual current position instead of a stale DB row.
    const auto dropX = static_cast<std::uint32_t>(session->player.x);
    const auto dropY = static_cast<std::uint32_t>(session->player.y);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::uint32_t> distrib(1, 1000000);
    const std::uint32_t groundId = distrib(gen);

    map->AddItem(GroundItem{
        .id = groundId,
        .x = dropX,
        .y = dropY,
        .item = droppedItem,
    });

    std::cout << "Dropped item_id " << itemId << " (qty " << droppedItem.quantity << ") from slot_index "
              << slotIndex << " as ground id " << groundId << "\n";

    PayloadWriter succWriter;
    ItemDropSuccess succResponse{
        .id = groundId,
        .x = dropX,
        .y = dropY,
        .item_id = itemId,
        .source_slot_id = request.slot_id,
        .new_item_id = remainingQuantity > 0 ? itemId : 0,
        .new_item_count = remainingQuantity > 0 ? remainingQuantity - 1 : 0,
    };
    succResponse.Serialize(succWriter);
    auto succData = succWriter.Data();

    GamePacket succPacket(GameOpcode::GC_ITEM_DROP_SUCC, succData);
    auto succPayload = succPacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, succPayload);
}

void HandleItemDelete(const GameContext& ctx, const ItemDelete& request)
{
    std::cout << "Item delete: slot_id " << request.slot_id << "\n";

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_ITEM_DELETE: socket has no resolved character (never entered)\n";
        return;
    }

    const int64_t characterId = session->characterId;

    // slot_id is the same wire-relative slot convention as ItemMove's
    // source_slot_id/dest_slot_id -- ItemRepository::ClearItemSlot resolves
    // the equipment/inventory split itself, no manual bag-offset conversion
    // needed here.
    auto slotContent = session->player.GetItemSlot(request.slot_id);
    if (!slotContent)
    {
        std::cout << "Rejecting CG_ITEM_DELETE: slot_id " << request.slot_id << " is empty\n";
        return;
    }

    const std::uint32_t itemId = slotContent->item_id;

    DatabaseTransaction txn(ctx.db);
    ItemRepository::ClearItemSlot(ctx.db, characterId, request.slot_id);
    txn.Commit();

    session->player.ClearItemSlot(request.slot_id);
    ctx.sessions.Set(ctx.clientSocket, *session);

    std::cout << "Deleted item_id " << itemId << " from slot_id " << request.slot_id << "\n";

    PayloadWriter succWriter;
    ItemDeleteSuccess succResponse{.slot_id = request.slot_id};
    succResponse.Serialize(succWriter);
    auto succData = succWriter.Data();

    GamePacket succPacket(GameOpcode::GC_ITEM_DELETE_SUCC, succData);
    auto succPayload = succPacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, succPayload);
}
