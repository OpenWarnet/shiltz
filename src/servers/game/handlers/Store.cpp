#include "Store.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
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

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>

namespace
{
// Flat fee charged on every CG_STORE_ITEM_OUT, regardless of how many
// units are withdrawn -- deposits (CG_STORE_ITEM_IN) are free.
constexpr std::int64_t kWithdrawalFee = 100;

// How the client displays a bank balance too large for one field -- see
// protocol/server/StoreMoneySucc.h.
constexpr std::int64_t kNegelValue = 100'000'000;

StoreMoneySucc BuildStoreMoneySucc(std::int64_t playerMoney, std::int64_t bankMoney)
{
    return StoreMoneySucc{
        .player_money = playerMoney,
        .bank_negel = static_cast<std::int32_t>(bankMoney / kNegelValue),
        .bank_remaining_cegel = static_cast<std::int32_t>(bankMoney % kNegelValue),
    };
}

std::optional<Item> GetBankSlot(const GameSession& session, std::uint32_t slotId)
{
    for (const auto& entry : session.bankItems)
    {
        if (entry.slot_id == slotId)
            return entry.item;
    }

    return std::nullopt;
}

void SetBankSlot(GameSession& session, std::uint32_t slotId, const Item& item)
{
    for (auto& entry : session.bankItems)
    {
        if (entry.slot_id == slotId)
        {
            entry.item = item;
            return;
        }
    }

    session.bankItems.push_back(BankRepository::SlotItem{.slot_id = slotId, .item = item});
}

void ClearBankSlot(GameSession& session, std::uint32_t slotId)
{
    std::erase_if(session.bankItems,
                   [slotId](const BankRepository::SlotItem& entry) { return entry.slot_id == slotId; });
}
} // namespace

void HandleStoreCreate(const GameContext& ctx, const StoreCreate& request)
{
    std::cout << "Store create request\n"; // password intentionally not logged

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_STORE_CREATE: socket has no resolved character (never entered)\n";
        return;
    }

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session are
    // copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session]() {
    // No GC_STORE_CREATE fail variant on the wire -- same silent-drop policy
    // as HandleStoreItemOut/In. Refuse to stomp an existing bank_accounts
    // row (and its password/contents) rather than silently re-provisioning it.
    if (BankRepository::FindAccount(ctx.db, session->accountId))
    {
        std::cout << "Rejecting CG_STORE_CREATE: bank_accounts row already exists for account "
                  << session->accountId << "\n";
        return;
    }

    auto account = BankRepository::CreateAccount(ctx.db, session->accountId, request.password);

    std::cout << "Created bank account " << account.id << " for account " << session->accountId << "\n";

    PayloadWriter writer;
    StoreCreateSucc{}.Serialize(writer);

    GamePacket packet(GameOpcode::GC_STORE_CREATE_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    });
}

void HandleStoreOpen(const GameContext& ctx, const StoreOpen& request)
{
    std::cout << "Store open request\n"; // password intentionally not logged

    auto sendFail = [ctx](std::int32_t reason = 1)
    {
        PayloadWriter failWriter;
        StoreOpenFail{.reason = reason}.Serialize(failWriter);

        GamePacket failPacket(GameOpcode::GC_STORE_OPEN_FAIL, failWriter.Data());
        ctx.server.SendTo(ctx.clientSocket, failPacket.Serialize(ctx.key));
    };

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_STORE_OPEN: socket has no resolved character (never entered)\n";
        return;
    }

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session/sendFail
    // are copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session, sendFail]() mutable {
    auto account = BankRepository::FindAccount(ctx.db, session->accountId);
    if (!account)
    {
        std::cout << "Rejecting CG_STORE_OPEN: no bank_accounts row for account " << session->accountId
                  << "\n";
        sendFail(2);
        return;
    }

    if (account->password != request.password)
    {
        std::cout << "Rejecting CG_STORE_OPEN: password mismatch for account " << session->accountId
                  << "\n";
        sendFail();
        return;
    }

    session->bankAccountId = account->id;
    session->bankItems = BankRepository::LoadAllItems(ctx.db, account->id);
    session->bankMoney = account->money;
    ctx.sessions.Set(ctx.clientSocket, *session);

    PayloadWriter writer;
    StoreOpenSucc response{};
    for (const auto& entry : session->bankItems)
    {
        if (entry.slot_id >= StoreOpenSucc::kSlotCount)
            continue; // out-of-range row -- shouldn't happen, don't write out of bounds

        response.slots[entry.slot_id] = BankItemSlot{
            .item_id = entry.item.item_id,
            .qty_or_refine = entry.item.WireQuantityOrRefine(),
            .option_bits = static_cast<std::int64_t>(entry.item.option_bits),
        };
    }
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_STORE_OPEN_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));

    std::cout << "Opened bank account " << account->id << " for character " << session->characterId
              << "\n";
    });
}

void HandleStorePwModify(const GameContext& ctx, const StorePwModify& request)
{
    std::cout << "Store password modify request\n"; // passwords intentionally not logged

    auto sendFail = [ctx]
    {
        PayloadWriter failWriter;
        StorePwModifyFail{}.Serialize(failWriter);

        GamePacket failPacket(GameOpcode::GC_STORE_PW_MODIFY_FAIL, failWriter.Data());
        ctx.server.SendTo(ctx.clientSocket, failPacket.Serialize(ctx.key));
    };

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_STORE_PW_MODIFY: socket has no resolved character (never entered)\n";
        return;
    }

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session/sendFail
    // are copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session, sendFail]() {
    // Independently re-resolves the bank_accounts row from account_id --
    // doesn't require CG_STORE_OPEN to have already succeeded on this
    // connection, the same way HandleStoreOpen does its own lookup rather
    // than trusting session->bankAccountId.
    auto account = BankRepository::FindAccount(ctx.db, session->accountId);
    if (!account)
    {
        std::cout << "Rejecting CG_STORE_PW_MODIFY: no bank_accounts row for account "
                  << session->accountId << "\n";
        sendFail();
        return;
    }

    if (account->password != request.old_password)
    {
        std::cout << "Rejecting CG_STORE_PW_MODIFY: old password mismatch for account "
                  << session->accountId << "\n";
        sendFail();
        return;
    }

    BankRepository::SavePassword(ctx.db, account->id, request.new_password);

    std::cout << "Changed bank password for account " << session->accountId << "\n";

    PayloadWriter writer;
    StorePwModifySucc{}.Serialize(writer);

    GamePacket packet(GameOpcode::GC_STORE_PW_MODIFY_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    });
}

void HandleStoreClose(const GameContext& ctx, const StoreClose& request)
{
    (void)request; // constant 1 -- read for framing only, nothing to act on

    std::cout << "Store close request\n";

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_STORE_CLOSE: socket has no resolved character (never entered)\n";
        return;
    }

    // Drop this connection's bank authorization -- a later CG_STORE_ITEM_IN/
    // OUT or CG_STORE_MONEY_IN/OUT must go through CG_STORE_OPEN again.
    session->bankAccountId.reset();
    session->bankItems.clear();
    session->bankMoney = 0;
    ctx.sessions.Set(ctx.clientSocket, *session);

    PayloadWriter writer;
    StoreCloseSucc{}.Serialize(writer);

    GamePacket packet(GameOpcode::GC_STORE_CLOSE_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
}

void HandleStoreItemOut(const GameContext& ctx, const StoreItemOut& request)
{
    std::cout << "Store item out: inventory_slot_id " << request.inventory_slot_id << ", bank_slot_id "
              << request.bank_slot_id << ", amount " << request.amount << "\n";

    // There's no GC_STORE_ITEM_OUT fail variant on the wire -- every
    // rejection below just drops the request silently, same as a malformed
    // CG_ITEM_CONFIRM_NPC_REQUEST slot.
    if (request.amount == 0)
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: amount 0\n";
        return;
    }

    if (request.bank_slot_id >= StoreOpenSucc::kSlotCount)
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: bank_slot_id " << request.bank_slot_id
                  << " out of range\n";
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: socket has no resolved character (never entered)\n";
        return;
    }

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session are
    // copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session]() mutable {
    if (!session->bankAccountId)
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: bank not open on this connection\n";
        return;
    }

    auto bankContent = GetBankSlot(*session, request.bank_slot_id);
    if (!bankContent)
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: bank_slot_id " << request.bank_slot_id
                  << " is empty\n";
        return;
    }

    // Character's wallet money, not bank_accounts.money -- see
    // handlers/Store.cpp's design note in the repository header.
    if (session->player.money < kWithdrawalFee)
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: character " << session->characterId
                  << " can't afford the " << kWithdrawalFee << " withdrawal fee\n";
        return;
    }

    const std::int64_t bankAccountId = *session->bankAccountId;
    const std::int64_t characterId = session->characterId;

    // Same wire-relative -> bag-relative conversion as HandleItemPickup/HandleItemDrop.
    const auto inventorySlotIndex =
        static_cast<std::uint32_t>(static_cast<int64_t>(request.inventory_slot_id) -
                                    static_cast<int64_t>(InventoryItemList::kBagStartSlot));

    // Trust the client-given slot but verify it's consistent with the
    // withdrawal -- empty is fine (new stack), holding the same item_id is
    // fine (stack onto it), anything else (different item, or either side
    // non-stackable while the other is occupied) means stale client state.
    auto existingInventory = session->player.GetInventorySlot(inventorySlotIndex);
    if (existingInventory &&
        (existingInventory->item_id != bankContent->item_id || existingInventory->has_refine_level ||
         bankContent->has_refine_level))
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: inventory slot_index " << inventorySlotIndex
                  << " can't receive item_id " << bankContent->item_id << " -- ignoring\n";
        return;
    }

    Item updatedInventory;
    Item remainingBank = *bankContent;
    std::uint32_t movedAmount;
    bool bankSlotCleared;

    if (!bankContent->has_refine_level)
    {
        // Explicit template argument dodges the Windows.h min/max macro
        // collision -- see HexDump.h for the same idiom.
        movedAmount = std::min<std::uint32_t>(request.amount, bankContent->quantity);

        if (existingInventory)
        {
            updatedInventory = *existingInventory;
            updatedInventory.quantity += movedAmount;
        }
        else
        {
            updatedInventory = *bankContent;
            updatedInventory.quantity = movedAmount;
        }

        bankSlotCleared = movedAmount >= bankContent->quantity;
        if (!bankSlotCleared)
            remainingBank.quantity = bankContent->quantity - movedAmount;
    }
    else
    {
        // Equippable-style item (refine_level, not stackable) -- always
        // moves whole, regardless of requested amount.
        movedAmount = 1;
        updatedInventory = *bankContent;
        bankSlotCleared = true;
    }

    // The inventory write, the bank write, and the fee debit must land
    // together, or a crash between them duplicates/loses the item or moves
    // it for free.
    DatabaseTransaction txn(ctx.db);

    if (!ItemRepository::SaveInventorySlot(ctx.db, characterId, inventorySlotIndex, existingInventory,
                                            updatedInventory))
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: inventory slot_index " << inventorySlotIndex
                  << " changed concurrently\n";
        return;
    }

    const bool bankWriteOk =
        bankSlotCleared
            ? BankRepository::ClearItemSlot(ctx.db, bankAccountId, request.bank_slot_id, bankContent)
            : BankRepository::SaveItemSlot(ctx.db, bankAccountId, request.bank_slot_id, bankContent,
                                            remainingBank);
    if (!bankWriteOk)
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: bank_slot_id " << request.bank_slot_id
                  << " changed concurrently\n";
        return;
    }

    // Authoritative fee check against the DB's *current* money -- the
    // session->player.money precheck above could be stale under the
    // pipelined-request race (see CharacterRepository.h).
    auto newPlayerMoney = CharacterRepository::TrySpendMoney(ctx.db, characterId, kWithdrawalFee);
    if (!newPlayerMoney)
    {
        std::cout << "Rejecting CG_STORE_ITEM_OUT: character " << characterId
                  << " can't afford the " << kWithdrawalFee << " withdrawal fee (stale cache)\n";
        return;
    }

    txn.Commit();

    // Only mirror into the cache / session store once the transaction is
    // actually durable -- see the equivalent comment in ItemConfirmNpc.cpp.
    session->player.money = *newPlayerMoney;
    session->player.SetInventorySlot(inventorySlotIndex, updatedInventory);
    if (bankSlotCleared)
        ClearBankSlot(*session, request.bank_slot_id);
    else
        SetBankSlot(*session, request.bank_slot_id, remainingBank);
    ctx.sessions.Set(ctx.clientSocket, *session);

    std::cout << "Withdrew " << movedAmount << "x item_id " << bankContent->item_id
              << " from bank_slot_id " << request.bank_slot_id << " to inventory_slot_id "
              << request.inventory_slot_id << "\n";

    PayloadWriter writer;
    StoreItemOutSuccess response{
        .inventory_slot_id = request.inventory_slot_id,
        .inventory_item_id = updatedInventory.item_id,
        .inventory_qty_or_refine = updatedInventory.WireQuantityOrRefine(),
        .inventory_option_bits = static_cast<std::int64_t>(updatedInventory.option_bits),
        .bank_slot_id = request.bank_slot_id,
        .bank_item_id = bankSlotCleared ? 0 : remainingBank.item_id,
        .bank_qty_or_refine = bankSlotCleared ? 0 : remainingBank.WireQuantityOrRefine(),
        .bank_option_bits =
            bankSlotCleared ? 0 : static_cast<std::int64_t>(remainingBank.option_bits),
        .money = session->player.money,
    };
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_STORE_ITEM_OUT, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    });
}

void HandleStoreItemIn(const GameContext& ctx, const StoreItemIn& request)
{
    std::cout << "Store item in: inventory_slot_id " << request.inventory_slot_id << ", bank_slot_id "
              << request.bank_slot_id << ", amount " << request.amount << "\n";

    // No GC_STORE_ITEM_IN fail variant either -- same silent-drop policy as
    // HandleStoreItemOut.
    if (request.amount == 0)
    {
        std::cout << "Rejecting CG_STORE_ITEM_IN: amount 0\n";
        return;
    }

    if (request.bank_slot_id >= StoreOpenSucc::kSlotCount)
    {
        std::cout << "Rejecting CG_STORE_ITEM_IN: bank_slot_id " << request.bank_slot_id
                  << " out of range\n";
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_STORE_ITEM_IN: socket has no resolved character (never entered)\n";
        return;
    }

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session are
    // copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session]() mutable {
    if (!session->bankAccountId)
    {
        std::cout << "Rejecting CG_STORE_ITEM_IN: bank not open on this connection\n";
        return;
    }

    const std::int64_t bankAccountId = *session->bankAccountId;
    const std::int64_t characterId = session->characterId;

    // Same wire-relative -> bag-relative conversion as HandleItemPickup/HandleItemDrop.
    const auto inventorySlotIndex =
        static_cast<std::uint32_t>(static_cast<int64_t>(request.inventory_slot_id) -
                                    static_cast<int64_t>(InventoryItemList::kBagStartSlot));

    auto existingInventory = session->player.GetInventorySlot(inventorySlotIndex);
    if (!existingInventory)
    {
        std::cout << "Rejecting CG_STORE_ITEM_IN: inventory slot_index " << inventorySlotIndex
                  << " is empty\n";
        return;
    }

    auto existingBank = GetBankSlot(*session, request.bank_slot_id);
    if (existingBank &&
        (existingBank->item_id != existingInventory->item_id || existingBank->has_refine_level ||
         existingInventory->has_refine_level))
    {
        std::cout << "Rejecting CG_STORE_ITEM_IN: bank_slot_id " << request.bank_slot_id
                  << " can't receive item_id " << existingInventory->item_id << " -- ignoring\n";
        return;
    }

    Item updatedBank;
    Item remainingInventory = *existingInventory;
    std::uint32_t movedAmount;
    bool inventorySlotCleared;

    if (!existingInventory->has_refine_level)
    {
        movedAmount = std::min<std::uint32_t>(request.amount, existingInventory->quantity);

        if (existingBank)
        {
            updatedBank = *existingBank;
            updatedBank.quantity += movedAmount;
        }
        else
        {
            updatedBank = *existingInventory;
            updatedBank.quantity = movedAmount;
        }

        inventorySlotCleared = movedAmount >= existingInventory->quantity;
        if (!inventorySlotCleared)
            remainingInventory.quantity = existingInventory->quantity - movedAmount;
    }
    else
    {
        // Equippable-style item -- always moves whole, regardless of
        // requested amount.
        movedAmount = 1;
        updatedBank = *existingInventory;
        inventorySlotCleared = true;
    }

    // Same all-or-nothing reasoning as HandleStoreItemOut, minus the fee.
    DatabaseTransaction txn(ctx.db);

    if (!BankRepository::SaveItemSlot(ctx.db, bankAccountId, request.bank_slot_id, existingBank,
                                       updatedBank))
    {
        std::cout << "Rejecting CG_STORE_ITEM_IN: bank_slot_id " << request.bank_slot_id
                  << " changed concurrently\n";
        return;
    }

    const bool inventoryWriteOk =
        inventorySlotCleared
            ? ItemRepository::ClearInventorySlot(ctx.db, characterId, inventorySlotIndex, existingInventory)
            : ItemRepository::SaveInventorySlot(ctx.db, characterId, inventorySlotIndex, existingInventory,
                                                 remainingInventory);
    if (!inventoryWriteOk)
    {
        std::cout << "Rejecting CG_STORE_ITEM_IN: inventory slot_index " << inventorySlotIndex
                  << " changed concurrently\n";
        return;
    }

    txn.Commit();

    SetBankSlot(*session, request.bank_slot_id, updatedBank);
    if (inventorySlotCleared)
        session->player.ClearInventorySlot(inventorySlotIndex);
    else
        session->player.SetInventorySlot(inventorySlotIndex, remainingInventory);
    ctx.sessions.Set(ctx.clientSocket, *session);

    std::cout << "Deposited " << movedAmount << "x item_id " << updatedBank.item_id
              << " from inventory_slot_id " << request.inventory_slot_id << " to bank_slot_id "
              << request.bank_slot_id << "\n";

    PayloadWriter writer;
    StoreItemInSuccess response{
        .inventory_slot_id = request.inventory_slot_id,
        .inventory_item_id = inventorySlotCleared ? 0 : remainingInventory.item_id,
        .inventory_qty_or_refine = inventorySlotCleared ? 0 : remainingInventory.WireQuantityOrRefine(),
        .inventory_option_bits =
            inventorySlotCleared ? 0 : static_cast<std::int64_t>(remainingInventory.option_bits),
        .bank_slot_id = request.bank_slot_id,
        .bank_item_id = updatedBank.item_id,
        .bank_qty_or_refine = updatedBank.WireQuantityOrRefine(),
        .bank_option_bits = static_cast<std::int64_t>(updatedBank.option_bits),
    };
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_STORE_ITEM_IN, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    });
}

void HandleStoreMoneyOut(const GameContext& ctx, const StoreMoneyOut& request)
{
    std::cout << "Store money out: amount " << request.amount << "\n";

    auto sendFail = [ctx]
    {
        PayloadWriter failWriter;
        StoreMoneyFail{}.Serialize(failWriter);

        GamePacket failPacket(GameOpcode::GC_STORE_MONEY_FAIL, failWriter.Data());
        ctx.server.SendTo(ctx.clientSocket, failPacket.Serialize(ctx.key));
    };

    if (request.amount <= 0)
    {
        std::cout << "Rejecting CG_STORE_MONEY_OUT: amount " << request.amount << " out of range\n";
        sendFail();
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_STORE_MONEY_OUT: socket has no resolved character (never entered)\n";
        sendFail();
        return;
    }

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session/sendFail
    // are copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session, sendFail]() mutable {
    if (!session->bankAccountId)
    {
        std::cout << "Rejecting CG_STORE_MONEY_OUT: bank not open on this connection\n";
        sendFail();
        return;
    }

    if (session->bankMoney < request.amount)
    {
        std::cout << "Rejecting CG_STORE_MONEY_OUT: bank account " << *session->bankAccountId
                  << " has " << session->bankMoney << ", needs " << request.amount << "\n";
        sendFail();
        return;
    }

    const std::int64_t bankAccountId = *session->bankAccountId;

    // The bank debit and the wallet credit must land together, or a crash
    // between them creates or destroys money. Authoritative check against
    // the DB's *current* bank balance -- the session->bankMoney precheck
    // above could be stale under the pipelined-request race (see
    // CharacterRepository.h).
    DatabaseTransaction txn(ctx.db);

    auto newBankMoney = BankRepository::TrySpendMoney(ctx.db, bankAccountId, request.amount);
    if (!newBankMoney)
    {
        std::cout << "Rejecting CG_STORE_MONEY_OUT: bank account " << bankAccountId
                  << " has insufficient money for " << request.amount << " (stale cache)\n";
        sendFail();
        return;
    }

    const std::int64_t newPlayerMoney =
        CharacterRepository::AddMoney(ctx.db, session->characterId, request.amount);

    txn.Commit();

    session->bankMoney = *newBankMoney;
    session->player.money = newPlayerMoney;
    ctx.sessions.Set(ctx.clientSocket, *session);

    std::cout << "Withdrew " << request.amount << " money from bank account " << bankAccountId
              << " (bank now " << *newBankMoney << ", player now " << newPlayerMoney << ")\n";

    PayloadWriter writer;
    StoreMoneySucc response = BuildStoreMoneySucc(newPlayerMoney, *newBankMoney);
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_STORE_MONEY_OUT_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    });
}

void HandleStoreMoneyIn(const GameContext& ctx, const StoreMoneyIn& request)
{
    std::cout << "Store money in: amount " << request.amount << "\n";

    auto sendFail = [ctx]
    {
        PayloadWriter failWriter;
        StoreMoneyFail{}.Serialize(failWriter);

        GamePacket failPacket(GameOpcode::GC_STORE_MONEY_FAIL, failWriter.Data());
        ctx.server.SendTo(ctx.clientSocket, failPacket.Serialize(ctx.key));
    };

    if (request.amount <= 0)
    {
        std::cout << "Rejecting CG_STORE_MONEY_IN: amount " << request.amount << " out of range\n";
        sendFail();
        return;
    }

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_STORE_MONEY_IN: socket has no resolved character (never entered)\n";
        sendFail();
        return;
    }

    // Everything below is blocking SQLite work -- run it on the DB pool
    // instead of the connection's reactor thread. request/session/sendFail
    // are copied by value so they stay valid once this handler returns;
    // server.SendTo() is safe to call from any thread.
    boost::asio::post(ctx.dbPool, [ctx, request, session, sendFail]() mutable {
    if (!session->bankAccountId)
    {
        std::cout << "Rejecting CG_STORE_MONEY_IN: bank not open on this connection\n";
        sendFail();
        return;
    }

    if (session->player.money < request.amount)
    {
        std::cout << "Rejecting CG_STORE_MONEY_IN: character " << session->characterId << " has "
                  << session->player.money << ", needs " << request.amount << "\n";
        sendFail();
        return;
    }

    const std::int64_t bankAccountId = *session->bankAccountId;

    // Same all-or-nothing reasoning as HandleStoreMoneyOut, and same
    // authoritative-check-against-current-DB-state reasoning.
    DatabaseTransaction txn(ctx.db);

    auto newPlayerMoney = CharacterRepository::TrySpendMoney(ctx.db, session->characterId, request.amount);
    if (!newPlayerMoney)
    {
        std::cout << "Rejecting CG_STORE_MONEY_IN: character " << session->characterId
                  << " has insufficient money for " << request.amount << " (stale cache)\n";
        sendFail();
        return;
    }

    const std::int64_t newBankMoney = BankRepository::AddMoney(ctx.db, bankAccountId, request.amount);

    txn.Commit();

    session->player.money = *newPlayerMoney;
    session->bankMoney = newBankMoney;
    ctx.sessions.Set(ctx.clientSocket, *session);

    std::cout << "Deposited " << request.amount << " money into bank account " << bankAccountId
              << " (bank now " << newBankMoney << ", player now " << *newPlayerMoney << ")\n";

    PayloadWriter writer;
    StoreMoneySucc response = BuildStoreMoneySucc(*newPlayerMoney, newBankMoney);
    response.Serialize(writer);

    GamePacket packet(GameOpcode::GC_STORE_MONEY_IN_SUCC, writer.Data());
    ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    });
}
