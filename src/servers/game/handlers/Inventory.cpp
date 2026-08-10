#include "Inventory.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/ItemDrop.h"
#include "protocol/client/ItemMove.h"
#include "protocol/client/ItemPickup.h"
#include "protocol/server/InventoryItemList.h"
#include "protocol/server/ItemDropSuccess.h"
#include "protocol/server/ItemMapNew.h"
#include "protocol/server/ItemMapRemove.h"
#include "protocol/server/ItemMoveFail.h"
#include "protocol/server/ItemMoveSuccess.h"
#include "protocol/server/ItemPickupSuccess.h"
#include "storage/IDatabase.h"
#include "storage/Transaction.h"
#include "world/Item.h"
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
    // equipment slot, which this table doesn't cover) lands on a negative
    // index that will simply never match a real row.
    const int64_t slotIndex =
        static_cast<int64_t>(request.slot_id) - static_cast<int64_t>(InventoryItemList::kBagStartSlot);

    // Atomically claim the ground item so two players racing the same
    // pickup can't both grant it to themselves. Claimed before the DB
    // write, so a failed write loses the item rather than duplicating it.
    auto groundItem = ctx.world.GetMap().TryTakeItem(request.id);
    if (!groundItem)
    {
        std::cout << "Rejecting CG_ITEM_PICKUP: no ground item with id " << request.id << "\n";
        return;
    }

    const std::uint32_t itemId = groundItem->item_id;

    DatabaseTransaction txn(ctx.db);

    // Trust the client-given slot rather than computing one server-side --
    // but verify it's actually consistent with the item being picked up:
    // empty is fine (new stack), holding the same item_id is fine (stack
    // onto it), holding a *different* item_id means the client's view of
    // its own inventory is stale/wrong, so ignore the request rather than
    // clobbering whatever's actually there.
    auto findSlot = ctx.db.Prepare(
        "SELECT item_id, quantity, refine_level FROM inventory_slot "
        "WHERE character_id = ? AND slot_index = ?");
    findSlot->Bind(0, characterId);
    findSlot->Bind(1, slotIndex);

    int64_t newQuantity = 0;
    SqlValue refineLevelColumn;

    if (findSlot->Step())
    {
        const auto existingItemId = static_cast<std::uint32_t>(std::get<int64_t>(findSlot->Column(0)));
        if (existingItemId != itemId)
        {
            std::cout << "Rejecting CG_ITEM_PICKUP: slot_index " << slotIndex << " holds item_id "
                      << existingItemId << ", not " << itemId << " -- ignoring\n";
            // Put the claimed item back rather than dropping it -- this is
            // a normal rejection (stale client state), not a failure.
            ctx.world.GetMap().AddItem(*groundItem);
            return;
        }

        const SqlValue quantityColumn = findSlot->Column(1);
        const int64_t currentQuantity =
            std::holds_alternative<int64_t>(quantityColumn) ? std::get<int64_t>(quantityColumn) : 0;
        newQuantity = currentQuantity + 1;
        refineLevelColumn = findSlot->Column(2);

        auto updateQuantity = ctx.db.Prepare(
            "UPDATE inventory_slot SET quantity = ? WHERE character_id = ? AND slot_index = ?");
        updateQuantity->Bind(0, newQuantity);
        updateQuantity->Bind(1, characterId);
        updateQuantity->Bind(2, slotIndex);
        updateQuantity->Step();

        std::cout << "Stacked item_id " << itemId << " at slot_index " << slotIndex << " (qty now "
                  << newQuantity << ")\n";
    }
    else
    {
        newQuantity = 1;
        // refineLevelColumn stays monostate -- the new row's refine_level
        // is NULL (the INSERT below doesn't set it).

        auto insertItem = ctx.db.Prepare(
            "INSERT INTO inventory_slot (character_id, slot_index, item_id, quantity) VALUES (?, ?, ?, 1)");
        insertItem->Bind(0, characterId);
        insertItem->Bind(1, slotIndex);
        insertItem->Bind(2, static_cast<int64_t>(itemId));
        insertItem->Step();

        std::cout << "Added item_id " << itemId << " at slot_index " << slotIndex << "\n";
    }

    txn.Commit();

    // Same dual-purpose wire field as InventoryItemSlot::qty_or_refine --
    // if the slot has a refine_level, that wins (used as-is); otherwise
    // it's a stackable item, shown as quantity - 1 (see InventoryItemList.h).
    const std::uint32_t qtyOrRefine = std::holds_alternative<int64_t>(refineLevelColumn)
                                           ? static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn))
                                           : static_cast<std::uint32_t>(newQuantity - 1);

    PayloadWriter succWriter;
    ItemPickupSuccess succResponse{
        .id = request.id,
        .slot_id = request.slot_id,
        .item_id = itemId,
        .qty_or_refine = qtyOrRefine,
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

namespace
{
    // Used by HandleItemDrop, which only ever touches inventory_slot --
    // see Player::LoadItemSlot for the equivalent covering both tables.
    struct SlotContent
    {
        int64_t itemId;
        SqlValue quantity;
        SqlValue refineLevel;
    };

    std::optional<SlotContent> FindInventoryContent(IDatabase& db, int64_t characterId, int64_t slotIndex)
    {
        auto stmt = db.Prepare(
            "SELECT item_id, quantity, refine_level FROM inventory_slot "
            "WHERE character_id = ? AND slot_index = ?");
        stmt->Bind(0, characterId);
        stmt->Bind(1, slotIndex);

        if (!stmt->Step())
            return std::nullopt;

        return SlotContent{
            .itemId = std::get<int64_t>(stmt->Column(0)),
            .quantity = stmt->Column(1),
            .refineLevel = stmt->Column(2),
        };
    }

    void DeleteSlotRow(IDatabase& db, int64_t characterId, int64_t slotIndex)
    {
        auto stmt = db.Prepare("DELETE FROM inventory_slot WHERE character_id = ? AND slot_index = ?");
        stmt->Bind(0, characterId);
        stmt->Bind(1, slotIndex);
        stmt->Step();
    }
} // namespace

void HandleItemMove(const GameContext& ctx, const ItemMove& request)
{
    std::cout << "Item move: source_slot_id " << request.source_slot_id << ", dest_slot_id "
              << request.dest_slot_id << "\n";

    auto sendFail = [&]
    {
        PayloadWriter failWriter;
        ItemMoveFail{}.Serialize(failWriter);
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

    // The two writes below must be all-or-nothing, or a crash between them
    // leaves the item's old and new slots both occupied (duplication).
    DatabaseTransaction txn(ctx.db);

    auto sourceContent = session->player.LoadItemSlot(ctx.db, request.source_slot_id);
    auto destContent = session->player.LoadItemSlot(ctx.db, request.dest_slot_id);

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
        session->player.SaveItemSlot(ctx.db, request.source_slot_id, *destContent);
        session->player.SaveItemSlot(ctx.db, request.dest_slot_id, *sourceContent);

        std::cout << "Swapped source_slot_id " << request.source_slot_id << " and dest_slot_id "
                  << request.dest_slot_id << "\n";
    }
    else if (sourceContent)
    {
        // dest is empty -- move source's content there and clear source.
        session->player.SaveItemSlot(ctx.db, request.dest_slot_id, *sourceContent);
        session->player.ClearItemSlot(ctx.db, request.source_slot_id);

        std::cout << "Moved source_slot_id " << request.source_slot_id << " to empty dest_slot_id "
                  << request.dest_slot_id << "\n";
    }
    else
    {
        // source is empty, dest occupied -- move the other way.
        session->player.SaveItemSlot(ctx.db, request.source_slot_id, *destContent);
        session->player.ClearItemSlot(ctx.db, request.dest_slot_id);

        std::cout << "Moved dest_slot_id " << request.dest_slot_id << " to empty source_slot_id "
                  << request.source_slot_id << "\n";
    }

    txn.Commit();

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

    const int64_t characterId = session->characterId;
    // Same wire-relative -> bag-relative conversion as HandleItemPickup --
    // see the comment there.
    const int64_t slotIndex =
        static_cast<int64_t>(request.slot_id) - static_cast<int64_t>(InventoryItemList::kBagStartSlot);

    // The slot removal must commit before the ground item is created --
    // otherwise a crash between the two loses or duplicates the item.
    DatabaseTransaction txn(ctx.db);

    auto slotContent = FindInventoryContent(ctx.db, characterId, slotIndex);
    if (!slotContent)
    {
        std::cout << "Rejecting CG_ITEM_DROP: slot_index " << slotIndex << " is empty\n";
        return;
    }

    const auto itemId = static_cast<std::uint32_t>(slotContent->itemId);

    std::uint32_t droppedQuantity = 1;
    std::uint32_t droppedRefineLevel = 0;
    // What's left in slotIndex after the drop -- 0 means the slot ended up
    // empty (fully dropped, or an equippable item, which always drops
    // whole).
    std::uint32_t remainingQuantity = 0;

    if (std::holds_alternative<int64_t>(slotContent->quantity))
    {
        const int64_t currentQuantity = std::get<int64_t>(slotContent->quantity);
        // Explicit template argument (not just std::min(...)) dodges the
        // Windows.h min/max macro collision -- see HexDump.h for the same
        // idiom; this project doesn't define NOMINMAX anywhere.
        const int64_t toDrop = std::min<int64_t>(static_cast<int64_t>(request.quantity), currentQuantity);
        droppedQuantity = static_cast<std::uint32_t>(toDrop);

        if (toDrop >= currentQuantity)
        {
            DeleteSlotRow(ctx.db, characterId, slotIndex);
        }
        else
        {
            remainingQuantity = static_cast<std::uint32_t>(currentQuantity - toDrop);

            auto updateSlot = ctx.db.Prepare(
                "UPDATE inventory_slot SET quantity = ? WHERE character_id = ? AND slot_index = ?");
            updateSlot->Bind(0, currentQuantity - toDrop);
            updateSlot->Bind(1, characterId);
            updateSlot->Bind(2, slotIndex);
            updateSlot->Step();
        }
    }
    else
    {
        // Equippable-style item (refine_level, not stackable) -- dropping
        // always removes the whole thing, regardless of requested quantity.
        if (std::holds_alternative<int64_t>(slotContent->refineLevel))
            droppedRefineLevel = static_cast<std::uint32_t>(std::get<int64_t>(slotContent->refineLevel));

        DeleteSlotRow(ctx.db, characterId, slotIndex);
    }

    txn.Commit();

    // Player::x/y is kept live by HandleMovement on every CG_MOVE, unlike
    // character_position (only written at creation) -- drop at the
    // session's actual current position instead of a stale DB row.
    const auto dropX = static_cast<std::uint32_t>(session->player.x);
    const auto dropY = static_cast<std::uint32_t>(session->player.y);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::uint32_t> distrib(1, 1000000);
    const std::uint32_t groundId = distrib(gen);

    ctx.world.GetMap().AddItem(Item{
        .id = groundId,
        .item_id = itemId,
        .x = dropX,
        .y = dropY,
        .quantity = droppedQuantity,
        .refine_level = droppedRefineLevel,
    });

    std::cout << "Dropped item_id " << itemId << " (qty " << droppedQuantity << ") from slot_index "
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
