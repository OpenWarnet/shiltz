#include "Trade.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "protocol/client/ItemTradeBuy.h"
#include "protocol/client/ItemTradeSell.h"
#include "protocol/server/InventoryItemList.h"
#include "protocol/server/TradeBuyFail.h"
#include "protocol/server/TradeBuySucc.h"
#include "protocol/server/TradeSellFail.h"
#include "protocol/server/TradeSellSucc.h"
#include "repositories/ItemRepository.h"
#include "storage/Transaction.h"
#include "tables/GameData.h"
#include "tables/ItemTable.h"
#include "tables/SellerTable.h"
#include "world/Item.h"

#include <algorithm>
#include <iostream>

void HandleItemTradeBuy(const GameContext& ctx, const ItemTradeBuy& request)
{
    std::cout << "Item trade buy: shop_id " << request.shop_id << ", item_buy_index "
              << request.item_buy_index << ", amount " << request.amount << ", slot_id " << request.slot_id
              << ", creature_instance_id " << request.creature_instance_id << "\n";

    auto sendFail = [ctx, request]
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

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session/sendFail
    // are copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session, sendFail]() mutable {
    const SellerRecord* seller = ctx.data.sellers.Find(request.shop_id);
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

    const ItemRecord* item = ctx.data.items.Find(itemId);
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
    const auto slotIndex = static_cast<std::uint32_t>(static_cast<int64_t>(request.slot_id) -
                                                        static_cast<int64_t>(InventoryItemList::kBagStartSlot));

    // The item write and the money debit must land together, or a crash
    // between them grants the item for free or debits with nothing to show.
    DatabaseTransaction txn(ctx.db);

    // Trust the client-given slot but verify it's consistent with the
    // purchase -- empty is fine (new stack), holding the same item_id is
    // fine (stack onto it), holding a *different* item_id means the
    // client's view of its own inventory is stale, so reject rather than
    // clobbering whatever's actually there. Checked before any money moves.
    // Read from the cache rather than the DB -- see repositories/ItemRepository.h.
    auto existing = session->player.GetInventorySlot(slotIndex);

    Item updated;
    if (existing)
    {
        if (existing->item_id != itemId)
        {
            std::cout << "Rejecting CG_ITEM_TRADE_BUY: slot_index " << slotIndex << " holds item_id "
                      << existing->item_id << ", not " << itemId << " -- ignoring\n";
            sendFail();
            return;
        }

        // Stack onto the existing slot -- item_level/option_bits stay the
        // existing stack's, same reasoning as HandleItemPickup.
        updated = *existing;
        updated.quantity += request.amount;

        std::cout << "Stacked item_id " << itemId << " at slot_index " << slotIndex << " (qty now "
                  << updated.quantity << ")\n";
    }
    else
    {
        updated = Item{.item_id = itemId, .quantity = request.amount};

        std::cout << "Added item_id " << itemId << " at slot_index " << slotIndex << " (qty "
                  << updated.quantity << ")\n";
    }

    ItemRepository::SaveInventorySlot(ctx.db, characterId, slotIndex, updated);

    session->player.money -= totalCost;
    session->player.SaveMoney(ctx.db);

    txn.Commit();

    // Only mirror into the cache / session store once the transaction is
    // actually durable -- see the equivalent comment in ItemConfirmNpc.cpp.
    session->player.SetInventorySlot(slotIndex, updated);
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
        .new_count = updated.WireQuantityOrRefine(),
        .option = 0,
        .option2 = 0,
        .money = session->player.money,
    };
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket packet(GameOpcode::GC_TRADE_BUY_SUCC, data);
    auto payload = packet.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, payload);
    });
}

void HandleItemTradeSell(const GameContext& ctx, const ItemTradeSell& request)
{
    std::cout << "Item trade sell: slot_id " << request.slot_id << ", count " << request.count
              << ", instance_id " << request.instance_id << "\n";

    auto sendFail = [ctx, request]
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

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session/sendFail
    // are copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session, sendFail]() mutable {
    const int64_t characterId = session->characterId;
    // Same wire-relative -> bag-relative conversion as HandleItemPickup/HandleItemDrop.
    const auto slotIndex = static_cast<std::uint32_t>(static_cast<int64_t>(request.slot_id) -
                                                        static_cast<int64_t>(InventoryItemList::kBagStartSlot));

    // Same reasoning as HandleItemTradeBuy -- the item removal and the
    // money credit must be one atomic unit.
    DatabaseTransaction txn(ctx.db);

    // Read from the cache rather than the DB -- see repositories/ItemRepository.h.
    auto existing = session->player.GetInventorySlot(slotIndex);
    if (!existing)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_SELL: slot_index " << slotIndex << " is empty\n";
        sendFail();
        return;
    }

    const std::uint32_t itemId = existing->item_id;

    const ItemRecord* item = ctx.data.items.Find(itemId);
    if (!item)
    {
        std::cout << "Rejecting CG_ITEM_TRADE_SELL: no item record for item_id " << itemId << "\n";
        sendFail();
        return;
    }

    std::uint32_t soldCount = 1;
    std::uint32_t remainingCount = 0;
    bool slotCleared = true;
    Item remainingItem;

    if (!existing->has_refine_level)
    {
        const std::uint32_t currentQuantity = existing->quantity;
        // Explicit template argument dodges the Windows.h min/max macro
        // collision -- see HexDump.h for the same idiom.
        const std::uint32_t toSell = std::min<std::uint32_t>(request.count, currentQuantity);
        soldCount = toSell;

        if (toSell >= currentQuantity)
        {
            ItemRepository::ClearInventorySlot(ctx.db, characterId, slotIndex);
        }
        else
        {
            remainingCount = currentQuantity - toSell;

            remainingItem = *existing;
            remainingItem.quantity = remainingCount;
            ItemRepository::SaveInventorySlot(ctx.db, characterId, slotIndex, remainingItem);
            slotCleared = false;
        }
    }
    else
    {
        // Equippable-style item (refine_level, not stackable) -- selling
        // always removes the whole thing, regardless of requested count.
        ItemRepository::ClearInventorySlot(ctx.db, characterId, slotIndex);
    }

    session->player.money += item->sell_price * static_cast<std::int64_t>(soldCount);
    session->player.SaveMoney(ctx.db);

    txn.Commit();

    // Only mirror into the cache / session store once the transaction is
    // actually durable -- see the equivalent comment in ItemConfirmNpc.cpp.
    if (slotCleared)
        session->player.ClearInventorySlot(slotIndex);
    else
        session->player.SetInventorySlot(slotIndex, remainingItem);
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
    });
}
