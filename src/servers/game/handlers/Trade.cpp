#include "Trade.h"

#include "Outbox.h"
#include "Persistence.h"
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
#include "world/Player.h"
#include "world/World.h"

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
} // namespace

void HandleItemTradeBuy(const GameContext& ctx, const ItemTradeBuy& request, Player& player)
{
    auto sendFail = [ctx] { ctx.outbox.Send(ctx.connection, TradeBuyFail{}); };

    if (request.amount == 0)
        return sendFail();

    const SellerRecord* seller = ctx.data.sellers.Find(request.shop_id);
    if (!seller || request.item_buy_index >= SellerRecord::kItemCount)
        return sendFail();

    const auto itemId = static_cast<std::uint32_t>(seller->items[request.item_buy_index]);
    const ItemRecord* item = ctx.data.items.Find(itemId);
    if (!item)
        return sendFail();

    const std::int64_t totalCost = item->buy_price * static_cast<std::int64_t>(request.amount);
    if (player.character.money < totalCost)
        return sendFail();

    const std::int64_t characterId = player.character.id;
    const std::uint32_t slotIndex = BagIndex(request.slot_id);
    const std::optional<Item> existing = player.character.GetInventorySlot(slotIndex);

    // A slot holding a different item means the client's view is stale: reject before any money moves.
    Item updated;
    if (existing)
    {
        if (existing->item_id != itemId)
            return sendFail();

        updated = *existing;
        updated.quantity += request.amount;
    }
    else
    {
        updated = Item{.item_id = itemId, .quantity = request.amount};
    }

    // The debit and the item land together; nullopt if money or the slot changed underneath.
    auto writes = [=](IDatabase& db) -> std::optional<std::int64_t>
    {
        DatabaseTransaction txn(db);
        auto newMoney = CharacterRepository::TrySpendMoney(db, characterId, totalCost);
        if (!newMoney || !ItemRepository::SaveInventorySlot(db, characterId, slotIndex, existing, updated))
            return std::nullopt;
        txn.Commit();
        return newMoney;
    };

    ctx.persistence.Run(
        writes,
        [ctx, request, slotIndex, itemId, updated, sendFail](std::optional<std::int64_t> newMoney)
        {
            if (!newMoney)
                return sendFail();

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                player->character.money = *newMoney;
                player->character.SetInventorySlot(slotIndex, updated);
            }

            TradeBuySucc response;
            response.slot_id = request.slot_id;
            response.item_id = itemId;
            response.new_count = updated.WireQuantityOrRefine();
            response.option = 0;
            response.option2 = 0;
            response.money = *newMoney;
            ctx.outbox.Send(ctx.connection, response);
        },
        [sendFail](const std::string&) { sendFail(); });
}

void HandleItemTradeSell(const GameContext& ctx, const ItemTradeSell& request, Player& player)
{
    auto sendFail = [ctx] { ctx.outbox.Send(ctx.connection, TradeSellFail{}); };

    if (request.count == 0)
        return sendFail();

    const std::int64_t characterId = player.character.id;
    const std::uint32_t slotIndex = BagIndex(request.slot_id);
    const std::optional<Item> existing = player.character.GetInventorySlot(slotIndex);
    if (!existing)
        return sendFail();

    const std::uint32_t itemId = existing->item_id;
    const ItemRecord* item = ctx.data.items.Find(itemId);
    if (!item)
        return sendFail();

    std::uint32_t soldCount = 1;
    std::uint32_t remainingCount = 0;
    Item remainingItem;

    // Equippable items always sell whole, regardless of the requested count.
    if (!existing->has_refine_level)
    {
        soldCount = std::min<std::uint32_t>(request.count, existing->quantity);
        remainingCount = existing->quantity - soldCount;
        remainingItem = *existing;
        remainingItem.quantity = remainingCount;
    }

    const bool slotCleared = remainingCount == 0;
    const std::int64_t earned = item->sell_price * static_cast<std::int64_t>(soldCount);

    // The removal and the credit land together; nullopt if the slot changed underneath.
    auto writes = [=](IDatabase& db) -> std::optional<std::int64_t>
    {
        DatabaseTransaction txn(db);
        const bool slotWritten =
            slotCleared ? ItemRepository::ClearInventorySlot(db, characterId, slotIndex, existing)
                        : ItemRepository::SaveInventorySlot(db, characterId, slotIndex, existing, remainingItem);
        if (!slotWritten)
            return std::nullopt;
        const std::int64_t newMoney = CharacterRepository::AddMoney(db, characterId, earned);
        txn.Commit();
        return newMoney;
    };

    ctx.persistence.Run(
        writes,
        [ctx, request, slotIndex, itemId, slotCleared, remainingItem, remainingCount,
         sendFail](std::optional<std::int64_t> newMoney)
        {
            if (!newMoney)
                return sendFail();

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                if (slotCleared)
                    player->character.ClearInventorySlot(slotIndex);
                else
                    player->character.SetInventorySlot(slotIndex, remainingItem);
                player->character.money = *newMoney;
            }

            // item_id 0 tells the client the slot is now empty.
            TradeSellSucc response;
            response.slot_id = request.slot_id;
            response.item_id = remainingCount > 0 ? itemId : 0;
            response.new_count = remainingCount > 0 ? remainingCount - 1 : 0;
            response.option = 0;
            response.option2 = 0;
            response.money_after = *newMoney;
            ctx.outbox.Send(ctx.connection, response);
        },
        [sendFail](const std::string&) { sendFail(); });
}
