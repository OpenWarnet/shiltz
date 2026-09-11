#include "Trade.h"

#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/Server.h"
#include "protocol/client/ItemTradeBuy.h"
#include "protocol/client/ItemTradeSell.h"
#include "protocol/server/InventoryItemList.h"
#include "protocol/server/TradeBuyFail.h"
#include "protocol/server/TradeBuySucc.h"
#include "protocol/server/TradeSellFail.h"
#include "protocol/server/TradeSellSucc.h"
#include "repositories/CharacterRepository.h"
#include "repositories/ItemRepository.h"
#include "storage/Transaction.h"
#include "tables/GameData.h"
#include "tables/ItemTable.h"
#include "tables/SellerTable.h"
#include "world/Item.h"

#include <algorithm>

void HandleItemTradeBuy(const GameContext& ctx, const ItemTradeBuy& request)
{
    auto sendFail = [ctx, request]
    { ctx.server.SendTo(ctx.clientSocket, TradeBuyFail{}.Packet().Serialize(ctx.key)); };

    if (request.amount == 0)
    {
        sendFail();
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
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
        sendFail();
        return;
    }

    if (request.item_buy_index >= SellerRecord::kItemCount)
    {
        sendFail();
        return;
    }

    const auto itemId = static_cast<std::uint32_t>(seller->items[request.item_buy_index]);

    const ItemRecord* item = ctx.data.items.Find(itemId);
    if (!item)
    {
        sendFail();
        return;
    }

    const std::int64_t totalCost = item->buy_price * static_cast<std::int64_t>(request.amount);
    if (session->character.money < totalCost)
    {
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
    auto existing = session->character.GetInventorySlot(slotIndex);

    Item updated;
    if (existing)
    {
        if (existing->item_id != itemId)
        {
            sendFail();
            return;
        }

        // Stack onto the existing slot -- item_level/option_bits stay the
        // existing stack's, same reasoning as HandleItemPickup.
        updated = *existing;
        updated.quantity += request.amount;
    }
    else
    {
        updated = Item{.item_id = itemId, .quantity = request.amount};
    }

    // Authoritative debit -- checked atomically against the DB's *current*
    // money rather than the cached session->character.money read above, which
    // could be stale under the pipelined-request race (see
    // CharacterRepository.h).
    auto newMoney = CharacterRepository::TrySpendMoney(ctx.db, characterId, totalCost);
    if (!newMoney)
    {
        sendFail();
        return;
    }

    // Guarded write: fails if some other pipelined request changed this
    // slot between the cache read above and now -- see ItemRepository.h.
    if (!ItemRepository::SaveInventorySlot(ctx.db, characterId, slotIndex, existing, updated))
    {
        sendFail();
        return;
    }

    txn.Commit();

    // Only mirror into the cache / session store once the transaction is
    // actually durable -- see the equivalent comment in ItemConfirmNpc.cpp.
    session->character.money = *newMoney;
    session->character.SetInventorySlot(slotIndex, updated);
    ctx.sessions.Set(ctx.clientSocket, *session);

    // Same dual-purpose wire convention as ItemPickupSuccess::qty_or_refine --
    // a stackable item's count is shown as quantity - 1 (0 = 1 item, 1 = 2
    // items, ...). option/option2 are still hardcoded placeholders -- this
    // path doesn't yet distinguish stackable items from equippable ones
    // with a refine_level.
    TradeBuySucc response;
    response.slot_id = request.slot_id;
    response.item_id = itemId;
    response.new_count = updated.WireQuantityOrRefine();
    response.option = 0;
    response.option2 = 0;
    response.money = session->character.money;
    ctx.server.SendTo(ctx.clientSocket, response.Packet().Serialize(ctx.key));
    });
}

void HandleItemTradeSell(const GameContext& ctx, const ItemTradeSell& request)
{
    auto sendFail = [ctx, request]
    { ctx.server.SendTo(ctx.clientSocket, TradeSellFail{}.Packet().Serialize(ctx.key)); };

    if (request.count == 0)
    {
        sendFail();
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
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
    auto existing = session->character.GetInventorySlot(slotIndex);
    if (!existing)
    {
        sendFail();
        return;
    }

    const std::uint32_t itemId = existing->item_id;

    const ItemRecord* item = ctx.data.items.Find(itemId);
    if (!item)
    {
        sendFail();
        return;
    }

    std::uint32_t soldCount = 1;
    std::uint32_t remainingCount = 0;
    bool slotCleared = true;
    Item remainingItem;

    // Guarded write: fails if some other pipelined request changed this
    // slot between the cache read above and now -- see ItemRepository.h.
    bool slotWriteOk;
    if (!existing->has_refine_level)
    {
        const std::uint32_t currentQuantity = existing->quantity;
        // Explicit template argument dodges the Windows.h min/max macro
        // collision -- see HexDump.h for the same idiom.
        const std::uint32_t toSell = std::min<std::uint32_t>(request.count, currentQuantity);
        soldCount = toSell;

        if (toSell >= currentQuantity)
        {
            slotWriteOk = ItemRepository::ClearInventorySlot(ctx.db, characterId, slotIndex, existing);
        }
        else
        {
            remainingCount = currentQuantity - toSell;

            remainingItem = *existing;
            remainingItem.quantity = remainingCount;
            slotWriteOk =
                ItemRepository::SaveInventorySlot(ctx.db, characterId, slotIndex, existing, remainingItem);
            slotCleared = false;
        }
    }
    else
    {
        // Equippable-style item (refine_level, not stackable) -- selling
        // always removes the whole thing, regardless of requested count.
        slotWriteOk = ItemRepository::ClearInventorySlot(ctx.db, characterId, slotIndex, existing);
    }

    if (!slotWriteOk)
    {
        sendFail();
        return;
    }

    const std::int64_t newMoney = CharacterRepository::AddMoney(
        ctx.db, characterId, item->sell_price * static_cast<std::int64_t>(soldCount));

    txn.Commit();

    // Only mirror into the cache / session store once the transaction is
    // actually durable -- see the equivalent comment in ItemConfirmNpc.cpp.
    if (slotCleared)
        session->character.ClearInventorySlot(slotIndex);
    else
        session->character.SetInventorySlot(slotIndex, remainingItem);
    session->character.money = newMoney;
    ctx.sessions.Set(ctx.clientSocket, *session);

    // remainingCount == 0 means the slot was emptied entirely (fully sold,
    // or an equippable item, which always sells whole) -- item_id 0 is how
    // the client is told to clear that slot, rather than sending a
    // qty_or_refine of 0 - 1 for a slot that no longer holds itemId at all.
    TradeSellSucc response;
    response.slot_id = request.slot_id;
    response.item_id = remainingCount > 0 ? itemId : 0;
    response.new_count = remainingCount > 0 ? remainingCount - 1 : 0;
    response.option = 0;
    response.option2 = 0;
    response.money_after = session->character.money;
    ctx.server.SendTo(ctx.clientSocket, response.Packet().Serialize(ctx.key));
    });
}
