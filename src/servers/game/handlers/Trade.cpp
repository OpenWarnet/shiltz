#include "Trade.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/ItemTradeBuy.h"
#include "protocol/client/ItemTradeSell.h"
#include "protocol/server/InventoryItemList.h"
#include "protocol/server/TradeBuyFail.h"
#include "protocol/server/TradeBuySucc.h"
#include "protocol/server/TradeSellFail.h"
#include "protocol/server/TradeSellSucc.h"
#include "storage/IDatabase.h"
#include "world/World.h"

#include <algorithm>
#include <iostream>

void HandleItemTradeBuy(const GameContext& ctx, const ItemTradeBuy& request)
{
    std::cout << "Item trade buy: shop_id " << request.shop_id << ", item_buy_index "
              << request.item_buy_index << ", amount " << request.amount << ", slot_id " << request.slot_id
              << ", creature_instance_id " << request.creature_instance_id << "\n";

    auto sendFail = [&]
    {
        PayloadWriter failWriter;
        TradeBuyFail{}.Serialize(failWriter);
        auto failData = failWriter.Data();

        GamePacket failPacket(GameOpcode::GC_TRADE_BUY_FAIL, failData);
        auto failPayload = failPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, failPayload);
    };

    if (request.amount == 0)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_BUY: amount 0\n";
        sendFail();
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_BUY: socket has no resolved character (never entered)\n";
        sendFail();
        return;
    }

    const SellerRecord* seller = ctx.world.FindSellerRecord(request.shop_id);
    if (!seller)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_BUY: no seller record for shop_id " << request.shop_id << "\n";
        sendFail();
        return;
    }

    if (request.item_buy_index >= SellerRecord::kItemCount)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_BUY: item_buy_index " << request.item_buy_index
                   << " out of range for shop_id " << request.shop_id << "\n";
        sendFail();
        return;
    }

    const auto itemId = static_cast<std::uint32_t>(seller->items[request.item_buy_index]);

    const ItemRecord* item = ctx.world.FindItemRecord(itemId);
    if (!item)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_BUY: no item record for item_id " << itemId << "\n";
        sendFail();
        return;
    }

    const std::int64_t totalCost = item->buy_price * static_cast<std::int64_t>(request.amount);
    if (session->player.money < totalCost)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_BUY: character " << session->characterId << " has "
                  << session->player.money << " money, needs " << totalCost << " for " << request.amount
                  << "x item_id " << itemId << "\n";
        sendFail();
        return;
    }

    const int64_t characterId = session->characterId;
    // Same wire-relative -> bag-relative conversion as HandleItemPickup/HandleItemDrop.
    const int64_t slotIndex =
        static_cast<int64_t>(request.slot_id) - static_cast<int64_t>(InventoryItemList::kBagStartSlot);

    // Trust the client-given slot but verify it's consistent with the
    // purchase -- empty is fine (new stack), holding the same item_id is
    // fine (stack onto it), holding a *different* item_id means the
    // client's view of its own inventory is stale, so reject rather than
    // clobbering whatever's actually there. Checked before any money moves.
    auto findSlot =
        ctx.db.Prepare("SELECT item_id, quantity FROM inventory_slot WHERE character_id = ? AND slot_index = ?");
    findSlot->Bind(0, characterId);
    findSlot->Bind(1, slotIndex);

    int64_t newQuantity = static_cast<int64_t>(request.amount);

    if (findSlot->Step())
    {
        const auto existingItemId = static_cast<std::uint32_t>(std::get<int64_t>(findSlot->Column(0)));
        if (existingItemId != itemId)
        {
            std::cout << "Rejecting CG_ITEM_TRADE_BUY: slot_index " << slotIndex << " holds item_id "
                      << existingItemId << ", not " << itemId << " -- ignoring\n";
            sendFail();
            return;
        }

        const SqlValue quantityColumn = findSlot->Column(1);
        const int64_t currentQuantity =
            std::holds_alternative<int64_t>(quantityColumn) ? std::get<int64_t>(quantityColumn) : 0;
        newQuantity = currentQuantity + static_cast<int64_t>(request.amount);

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
        auto insertItem = ctx.db.Prepare(
            "INSERT INTO inventory_slot (character_id, slot_index, item_id, quantity) VALUES (?, ?, ?, ?)");
        insertItem->Bind(0, characterId);
        insertItem->Bind(1, slotIndex);
        insertItem->Bind(2, static_cast<int64_t>(itemId));
        insertItem->Bind(3, newQuantity);
        insertItem->Step();

        std::cout << "Added item_id " << itemId << " at slot_index " << slotIndex << " (qty " << newQuantity
                  << ")\n";
    }

    session->player.money -= totalCost;
    ctx.sessions.Set(ctx.clientSocket, *session);

    // Same dual-purpose wire convention as ItemPickupSuccess::qty_or_refine --
    // a stackable item's count is shown as quantity - 1 (0 = 1 item, 1 = 2
    // items, ...). option/option2 are still hardcoded placeholders -- this
    // path doesn't yet distinguish stackable items from equippable ones
    // with a refine_level.
    PayloadWriter writer;
    TradeBuySucc response{
        .slot_id = request.slot_id,
        .item_id = itemId,
        .new_count = static_cast<std::uint32_t>(newQuantity - 1),
        .option = 0,
        .option2 = 0,
        .money = session->player.money,
    };
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket packet(GameOpcode::GC_TRADE_BUY_SUCC, data);
    auto payload = packet.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, payload);
}

void HandleItemTradeSell(const GameContext& ctx, const ItemTradeSell& request)
{
    std::cout << "Item trade sell: slot_id " << request.slot_id << ", count " << request.count
              << ", instance_id " << request.instance_id << "\n";

    auto sendFail = [&]
    {
        PayloadWriter failWriter;
        TradeSellFail{}.Serialize(failWriter);
        auto failData = failWriter.Data();

        GamePacket failPacket(GameOpcode::GC_TRADE_SELL_FAIL, failData);
        auto failPayload = failPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, failPayload);
    };

    if (request.count == 0)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_SELL: count 0\n";
        sendFail();
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_SELL: socket has no resolved character (never entered)\n";
        sendFail();
        return;
    }

    const int64_t characterId = session->characterId;
    // Same wire-relative -> bag-relative conversion as HandleItemPickup/HandleItemDrop.
    const int64_t slotIndex =
        static_cast<int64_t>(request.slot_id) - static_cast<int64_t>(InventoryItemList::kBagStartSlot);

    auto findSlot = ctx.db.Prepare(
        "SELECT item_id, quantity, refine_level FROM inventory_slot "
        "WHERE character_id = ? AND slot_index = ?");
    findSlot->Bind(0, characterId);
    findSlot->Bind(1, slotIndex);

    if (!findSlot->Step())
    {
        std::cout << "Rejecting CG_ITEM_TRADE_SELL: slot_index " << slotIndex << " is empty\n";
        sendFail();
        return;
    }

    const auto itemId = static_cast<std::uint32_t>(std::get<int64_t>(findSlot->Column(0)));
    const SqlValue quantityColumn = findSlot->Column(1);

    const ItemRecord* item = ctx.world.FindItemRecord(itemId);
    if (!item)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_SELL: no item record for item_id " << itemId << "\n";
        sendFail();
        return;
    }

    std::uint32_t soldCount = 1;
    std::uint32_t remainingCount = 0;

    if (std::holds_alternative<int64_t>(quantityColumn))
    {
        const int64_t currentQuantity = std::get<int64_t>(quantityColumn);
        // Explicit template argument dodges the Windows.h min/max macro
        // collision -- see HexDump.h for the same idiom.
        const int64_t toSell = std::min<int64_t>(static_cast<int64_t>(request.count), currentQuantity);
        soldCount = static_cast<std::uint32_t>(toSell);

        if (toSell >= currentQuantity)
        {
            auto deleteSlot =
                ctx.db.Prepare("DELETE FROM inventory_slot WHERE character_id = ? AND slot_index = ?");
            deleteSlot->Bind(0, characterId);
            deleteSlot->Bind(1, slotIndex);
            deleteSlot->Step();
        }
        else
        {
            remainingCount = static_cast<std::uint32_t>(currentQuantity - toSell);

            auto updateSlot = ctx.db.Prepare(
                "UPDATE inventory_slot SET quantity = ? WHERE character_id = ? AND slot_index = ?");
            updateSlot->Bind(0, currentQuantity - toSell);
            updateSlot->Bind(1, characterId);
            updateSlot->Bind(2, slotIndex);
            updateSlot->Step();
        }
    }
    else
    {
        // Equippable-style item (refine_level, not stackable) -- selling
        // always removes the whole thing, regardless of requested count.
        auto deleteSlot =
            ctx.db.Prepare("DELETE FROM inventory_slot WHERE character_id = ? AND slot_index = ?");
        deleteSlot->Bind(0, characterId);
        deleteSlot->Bind(1, slotIndex);
        deleteSlot->Step();
    }

    session->player.money += item->sell_price * static_cast<std::int64_t>(soldCount);
    ctx.sessions.Set(ctx.clientSocket, *session);

    std::cout << "Sold " << soldCount << "x item_id " << itemId << " from slot_index " << slotIndex
              << " for " << item->sell_price * static_cast<std::int64_t>(soldCount) << " money\n";

    // remainingCount == 0 means the slot was emptied entirely (fully sold,
    // or an equippable item, which always sells whole) -- item_id 0 is how
    // the client is told to clear that slot, rather than sending a
    // qty_or_refine of 0 - 1 for a slot that no longer holds itemId at all.
    PayloadWriter writer;
    TradeSellSucc response{
        .slot_id = request.slot_id,
        .item_id = remainingCount > 0 ? itemId : 0,
        .new_count = remainingCount > 0 ? remainingCount - 1 : 0,
        .option = 0,
        .option2 = 0,
        .money_after = session->player.money,
    };
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket packet(GameOpcode::GC_TRADE_SELL_SUCC, data);
    auto payload = packet.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, payload);
}
