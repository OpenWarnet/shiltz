#include "Store.h"

#include "Outbox.h"
#include "Persistence.h"
#include "protocol/client/StoreClose.h"
#include "protocol/client/StoreCreate.h"
#include "protocol/client/StoreItemIn.h"
#include "protocol/client/StoreItemOut.h"
#include "protocol/client/StoreMoneyIn.h"
#include "protocol/client/StoreMoneyOut.h"
#include "protocol/client/StoreOpen.h"
#include "protocol/client/StorePwModify.h"
#include "protocol/server/InventoryItemList.h"
#include "protocol/server/StoreCloseSucc.h"
#include "protocol/server/StoreCreateSucc.h"
#include "protocol/server/StoreItemInSuccess.h"
#include "protocol/server/StoreItemOutSuccess.h"
#include "protocol/server/StoreMoneyFail.h"
#include "protocol/server/StoreMoneySucc.h"
#include "protocol/server/StoreOpenFail.h"
#include "protocol/server/StoreOpenSucc.h"
#include "protocol/server/StorePwModifyFail.h"
#include "protocol/server/StorePwModifySucc.h"
#include "repositories/BankRepository.h"
#include "repositories/CharacterRepository.h"
#include "repositories/ItemRepository.h"
#include "storage/Transaction.h"
#include "world/Item.h"
#include "world/Player.h"
#include "world/World.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace
{
// Flat fee charged on every CG_STORE_ITEM_OUT, regardless of how many
// units are withdrawn -- deposits (CG_STORE_ITEM_IN) are free.
constexpr std::int64_t kWithdrawalFee = 100;

// How the client displays a bank balance too large for one field -- see
// protocol/server/StoreMoneySucc.h.
constexpr std::int64_t kNegelValue = 100'000'000;

// Response is StoreMoneyInSucc or StoreMoneyOutSucc -- same fields.
template <typename Response>
Response BuildStoreMoneySucc(std::int64_t characterMoney, std::int64_t bankMoney)
{
    Response response;
    response.player_money = characterMoney;
    response.bank_negel = static_cast<std::int32_t>(bankMoney / kNegelValue);
    response.bank_remaining_cegel = static_cast<std::int32_t>(bankMoney % kNegelValue);
    return response;
}

// Wire slot ids count bag slots from InventoryItemList::kBagStartSlot; inventory_slot is 0-based.
std::uint32_t BagIndex(std::uint32_t wireSlotId)
{
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(wireSlotId) -
                                      static_cast<std::int64_t>(InventoryItemList::kBagStartSlot));
}

// Sends an empty-bodied store reply; copyable into DB job callbacks.
template <typename Response> auto Reply(const GameContext& ctx)
{
    return [ctx] { ctx.outbox.Send(ctx.connection, Response{}); };
}

struct MoneyAfter
{
    std::int64_t character = 0;
    std::int64_t bank = 0;
};

// Applies a committed transfer to the player and replies with the new balances.
template <typename Response> void FinishTransfer(const GameContext& ctx, std::int64_t bankId, MoneyAfter after)
{
    if (Player* player = ctx.world.FindPlayer(ctx.connection))
    {
        player->character.money = after.character;
        if (player->bank && player->bank->id == bankId)
            player->bank->money = after.bank;
    }
    ctx.outbox.Send(ctx.connection, BuildStoreMoneySucc<Response>(after.character, after.bank));
}
} // namespace

void HandleStoreCreate(const GameContext& ctx, const StoreCreate& request, Player& player)
{
    // No GC_STORE_CREATE fail variant on the wire; an existing account is never re-provisioned.
    auto create = [accountId = player.account_id, password = request.password](IDatabase& db)
    {
        if (BankRepository::FindAccount(db, accountId))
            return false;
        BankRepository::CreateAccount(db, accountId, password);
        return true;
    };

    ctx.persistence.Run(
        create,
        [ctx](bool created)
        {
            if (created)
                Reply<StoreCreateSucc>(ctx)();
        },
        [](const std::string&) {});
}

void HandleStoreOpen(const GameContext& ctx, const StoreOpen& request, Player& player)
{
    auto sendFail = [ctx](std::int32_t reason)
    {
        StoreOpenFail response;
        response.reason = reason;
        ctx.outbox.Send(ctx.connection, response);
    };

    struct Opened
    {
        std::int32_t failReason = 0;
        Bank bank;
    };

    auto open = [accountId = player.account_id, password = request.password](IDatabase& db)
    {
        auto account = BankRepository::FindAccount(db, accountId);
        if (!account)
            return Opened{.failReason = 2};
        if (account->password != password)
            return Opened{.failReason = 1};
        return Opened{.bank = Bank{.id = account->id,
                                   .money = account->money,
                                   .items = BankRepository::LoadAllItems(db, account->id)}};
    };

    ctx.persistence.Run(
        open,
        [ctx, sendFail](Opened opened)
        {
            if (opened.failReason != 0)
                return sendFail(opened.failReason);

            StoreOpenSucc response{};
            for (const auto& entry : opened.bank.items)
            {
                if (entry.slot_id >= StoreOpenSucc::kSlotCount)
                    continue; // out-of-range row -- shouldn't happen, don't write out of bounds

                response.slots[entry.slot_id] = BankItemSlot{
                    .item_id = entry.item.item_id,
                    .qty_or_refine = entry.item.WireQuantityOrRefine(),
                    .option_bits = static_cast<std::int64_t>(entry.item.option_bits),
                };
            }

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
                player->bank = std::move(opened.bank);

            ctx.outbox.Send(ctx.connection, response);
        },
        [sendFail](const std::string&) { sendFail(1); });
}

void HandleStorePwModify(const GameContext& ctx, const StorePwModify& request, Player& player)
{
    // Looks the account up itself, so it doesn't need CG_STORE_OPEN first.
    auto modify = [accountId = player.account_id, request](IDatabase& db)
    {
        auto account = BankRepository::FindAccount(db, accountId);
        if (!account || account->password != request.old_password)
            return false;
        BankRepository::SavePassword(db, account->id, request.new_password);
        return true;
    };

    auto sendFail = Reply<StorePwModifyFail>(ctx);
    ctx.persistence.Run(
        modify,
        [ctx, sendFail](bool modified) { modified ? Reply<StorePwModifySucc>(ctx)() : sendFail(); },
        [sendFail](const std::string&) { sendFail(); });
}

void HandleStoreClose(const GameContext& ctx, const StoreClose& request, Player& player)
{
    (void)request; // constant 1 -- read for framing only, nothing to act on

    // Later item/money requests must go through CG_STORE_OPEN again.
    player.bank.reset();
    ctx.outbox.Send(ctx.connection, StoreCloseSucc{});
}

void HandleStoreItemOut(const GameContext& ctx, const StoreItemOut& request, Player& player)
{
    // No GC_STORE_ITEM_OUT fail variant on the wire: every rejection is a silent drop.
    if (request.amount == 0 || request.bank_slot_id >= StoreOpenSucc::kSlotCount || !player.bank)
        return;

    auto bankContent = player.bank->GetSlot(request.bank_slot_id);
    if (!bankContent)
        return;

    // The fee comes from the character's wallet, not the bank.
    if (player.character.money < kWithdrawalFee)
        return;

    const std::int64_t bankId = player.bank->id;
    const std::int64_t characterId = player.character.id;
    const std::uint32_t inventorySlotIndex = BagIndex(request.inventory_slot_id);

    // Stacking needs the same stackable item on both sides; anything else means stale client state.
    auto existingInventory = player.character.GetInventorySlot(inventorySlotIndex);
    if (existingInventory &&
        (existingInventory->item_id != bankContent->item_id || existingInventory->has_refine_level ||
         bankContent->has_refine_level))
        return;

    Item updatedInventory = *bankContent;
    Item remainingBank = *bankContent;
    bool bankSlotCleared = true;

    // Equippable items always move whole, regardless of the requested amount.
    if (!bankContent->has_refine_level)
    {
        const std::uint32_t movedAmount = std::min<std::uint32_t>(request.amount, bankContent->quantity);
        if (existingInventory)
        {
            updatedInventory = *existingInventory;
            updatedInventory.quantity += movedAmount;
        }
        else
        {
            updatedInventory.quantity = movedAmount;
        }

        bankSlotCleared = movedAmount >= bankContent->quantity;
        if (!bankSlotCleared)
            remainingBank.quantity = bankContent->quantity - movedAmount;
    }

    const std::uint32_t bankSlotId = request.bank_slot_id;

    // Inventory, bank and fee land together; nullopt if a slot or the money changed underneath.
    auto writes = [=](IDatabase& db) -> std::optional<std::int64_t>
    {
        DatabaseTransaction txn(db);
        if (!ItemRepository::SaveInventorySlot(db, characterId, inventorySlotIndex, existingInventory,
                                               updatedInventory))
            return std::nullopt;

        const bool bankWritten =
            bankSlotCleared ? BankRepository::ClearItemSlot(db, bankId, bankSlotId, bankContent)
                            : BankRepository::SaveItemSlot(db, bankId, bankSlotId, bankContent, remainingBank);
        if (!bankWritten)
            return std::nullopt;

        auto newMoney = CharacterRepository::TrySpendMoney(db, characterId, kWithdrawalFee);
        if (!newMoney)
            return std::nullopt;
        txn.Commit();
        return newMoney;
    };

    ctx.persistence.Run(
        writes,
        [=](std::optional<std::int64_t> newMoney)
        {
            if (!newMoney)
                return;

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                player->character.money = *newMoney;
                player->character.SetInventorySlot(inventorySlotIndex, updatedInventory);
                if (player->bank && player->bank->id == bankId)
                {
                    if (bankSlotCleared)
                        player->bank->ClearSlot(bankSlotId);
                    else
                        player->bank->SetSlot(bankSlotId, remainingBank);
                }
            }

            StoreItemOutSuccess response;
            response.inventory_slot_id = request.inventory_slot_id;
            response.inventory_item_id = updatedInventory.item_id;
            response.inventory_qty_or_refine = updatedInventory.WireQuantityOrRefine();
            response.inventory_option_bits = static_cast<std::int64_t>(updatedInventory.option_bits);
            response.bank_slot_id = bankSlotId;
            response.bank_item_id = bankSlotCleared ? 0 : remainingBank.item_id;
            response.bank_qty_or_refine = bankSlotCleared ? 0 : remainingBank.WireQuantityOrRefine();
            response.bank_option_bits = bankSlotCleared ? 0 : static_cast<std::int64_t>(remainingBank.option_bits);
            response.money = *newMoney;
            ctx.outbox.Send(ctx.connection, response);
        },
        [](const std::string&) {});
}

void HandleStoreItemIn(const GameContext& ctx, const StoreItemIn& request, Player& player)
{
    // No GC_STORE_ITEM_IN fail variant either: every rejection is a silent drop.
    if (request.amount == 0 || request.bank_slot_id >= StoreOpenSucc::kSlotCount || !player.bank)
        return;

    const std::int64_t bankId = player.bank->id;
    const std::int64_t characterId = player.character.id;
    const std::uint32_t inventorySlotIndex = BagIndex(request.inventory_slot_id);

    auto existingInventory = player.character.GetInventorySlot(inventorySlotIndex);
    if (!existingInventory)
        return;

    auto existingBank = player.bank->GetSlot(request.bank_slot_id);
    if (existingBank &&
        (existingBank->item_id != existingInventory->item_id || existingBank->has_refine_level ||
         existingInventory->has_refine_level))
        return;

    Item updatedBank = *existingInventory;
    Item remainingInventory = *existingInventory;
    bool inventorySlotCleared = true;

    // Equippable items always move whole, regardless of the requested amount.
    if (!existingInventory->has_refine_level)
    {
        const std::uint32_t movedAmount = std::min<std::uint32_t>(request.amount, existingInventory->quantity);
        if (existingBank)
        {
            updatedBank = *existingBank;
            updatedBank.quantity += movedAmount;
        }
        else
        {
            updatedBank.quantity = movedAmount;
        }

        inventorySlotCleared = movedAmount >= existingInventory->quantity;
        if (!inventorySlotCleared)
            remainingInventory.quantity = existingInventory->quantity - movedAmount;
    }

    const std::uint32_t bankSlotId = request.bank_slot_id;

    // Bank and inventory land together; false if either slot changed underneath.
    auto writes = [=](IDatabase& db)
    {
        DatabaseTransaction txn(db);
        if (!BankRepository::SaveItemSlot(db, bankId, bankSlotId, existingBank, updatedBank))
            return false;

        const bool inventoryWritten =
            inventorySlotCleared
                ? ItemRepository::ClearInventorySlot(db, characterId, inventorySlotIndex, existingInventory)
                : ItemRepository::SaveInventorySlot(db, characterId, inventorySlotIndex, existingInventory,
                                                    remainingInventory);
        if (!inventoryWritten)
            return false;
        txn.Commit();
        return true;
    };

    ctx.persistence.Run(
        writes,
        [=](bool saved)
        {
            if (!saved)
                return;

            if (Player* player = ctx.world.FindPlayer(ctx.connection))
            {
                if (player->bank && player->bank->id == bankId)
                    player->bank->SetSlot(bankSlotId, updatedBank);
                if (inventorySlotCleared)
                    player->character.ClearInventorySlot(inventorySlotIndex);
                else
                    player->character.SetInventorySlot(inventorySlotIndex, remainingInventory);
            }

            StoreItemInSuccess response;
            response.inventory_slot_id = request.inventory_slot_id;
            response.inventory_item_id = inventorySlotCleared ? 0 : remainingInventory.item_id;
            response.inventory_qty_or_refine = inventorySlotCleared ? 0 : remainingInventory.WireQuantityOrRefine();
            response.inventory_option_bits =
                inventorySlotCleared ? 0 : static_cast<std::int64_t>(remainingInventory.option_bits);
            response.bank_slot_id = bankSlotId;
            response.bank_item_id = updatedBank.item_id;
            response.bank_qty_or_refine = updatedBank.WireQuantityOrRefine();
            response.bank_option_bits = static_cast<std::int64_t>(updatedBank.option_bits);
            ctx.outbox.Send(ctx.connection, response);
        },
        [](const std::string&) {});
}

void HandleStoreMoneyOut(const GameContext& ctx, const StoreMoneyOut& request, Player& player)
{
    auto sendFail = Reply<StoreMoneyFail>(ctx);

    if (request.amount <= 0 || !player.bank || player.bank->money < request.amount)
        return sendFail();

    const std::int64_t bankId = player.bank->id;
    const std::int64_t characterId = player.character.id;
    const std::int64_t amount = request.amount;

    // Debit and credit land together, checked against the bank's current balance.
    auto writes = [=](IDatabase& db) -> std::optional<MoneyAfter>
    {
        DatabaseTransaction txn(db);
        auto newBankMoney = BankRepository::TrySpendMoney(db, bankId, amount);
        if (!newBankMoney)
            return std::nullopt;
        const std::int64_t newCharacterMoney = CharacterRepository::AddMoney(db, characterId, amount);
        txn.Commit();
        return MoneyAfter{.character = newCharacterMoney, .bank = *newBankMoney};
    };

    ctx.persistence.Run(
        writes,
        [ctx, bankId, sendFail](std::optional<MoneyAfter> after)
        { after ? FinishTransfer<StoreMoneyOutSucc>(ctx, bankId, *after) : sendFail(); },
        [sendFail](const std::string&) { sendFail(); });
}

void HandleStoreMoneyIn(const GameContext& ctx, const StoreMoneyIn& request, Player& player)
{
    auto sendFail = Reply<StoreMoneyFail>(ctx);

    if (request.amount <= 0 || !player.bank || player.character.money < request.amount)
        return sendFail();

    const std::int64_t bankId = player.bank->id;
    const std::int64_t characterId = player.character.id;
    const std::int64_t amount = request.amount;

    // Debit and credit land together, checked against the wallet's current balance.
    auto writes = [=](IDatabase& db) -> std::optional<MoneyAfter>
    {
        DatabaseTransaction txn(db);
        auto newCharacterMoney = CharacterRepository::TrySpendMoney(db, characterId, amount);
        if (!newCharacterMoney)
            return std::nullopt;
        const std::int64_t newBankMoney = BankRepository::AddMoney(db, bankId, amount);
        txn.Commit();
        return MoneyAfter{.character = *newCharacterMoney, .bank = newBankMoney};
    };

    ctx.persistence.Run(
        writes,
        [ctx, bankId, sendFail](std::optional<MoneyAfter> after)
        { after ? FinishTransfer<StoreMoneyInSucc>(ctx, bankId, *after) : sendFail(); },
        [sendFail](const std::string&) { sendFail(); });
}
