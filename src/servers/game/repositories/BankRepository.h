#pragma once

#include "world/Item.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class IDatabase;

// Owns every DB access for the "bank" storage feature (bank_accounts /
// bank_items) -- handlers/Store.cpp is this namespace's only caller.
namespace BankRepository
{
    // This game server instance's own server_id, matching the
    // single-server-setup convention used elsewhere (see
    // CharacterDataLoad::origin_server_id).
    inline constexpr std::int64_t kServerId = 0;

    struct Account
    {
        std::int64_t id = 0;
        std::string password;
        std::int64_t money = 0;
    };

    struct SlotItem
    {
        std::uint32_t slot_id = 0;
        Item item;
    };

    // Looks up accountId's bank_accounts row for this server.
    std::optional<Account> FindAccount(IDatabase& db, std::int64_t accountId);

    // Provisions a new bank_accounts row for accountId on this server,
    // starting at money = 0. Caller (handlers/Store.cpp's HandleStoreCreate)
    // is responsible for checking FindAccount() first -- this doesn't guard
    // against a duplicate row.
    Account CreateAccount(IDatabase& db, std::int64_t accountId, const std::string& password);

    // Relative, same reasoning as CharacterRepository::TrySpendMoney/
    // AddMoney -- two queued writes for the same bank account (see
    // Persistence.h) compose correctly instead of one clobbering the
    // other. TrySpendMoney only applies if current money >= amount; nullopt
    // (no write applied) means insufficient, and the caller must treat that
    // as a real-time rejection.
    std::optional<std::int64_t> TrySpendMoney(IDatabase& db, std::int64_t bankAccountId,
                                               std::int64_t amount);
    std::int64_t AddMoney(IDatabase& db, std::int64_t bankAccountId, std::int64_t amount);

    // Persists bank_accounts.password alone -- called by
    // handlers/Store.cpp's HandleStorePwModify.
    void SavePassword(IDatabase& db, std::int64_t bankAccountId, const std::string& password);

    std::vector<SlotItem> LoadAllItems(IDatabase& db, std::int64_t bankAccountId);

    // Compare-and-swap, same shape and reasoning as
    // ItemRepository::SaveInventorySlot/ClearInventorySlot --
    // expectedPrevious is what the caller's cache believes is in the slot
    // (nullopt = believes it's empty); the write only applies if the DB's
    // actual current content still matches. Returns false (no write
    // applied) on a mismatch. Upserts by (bank_accounts_id, slot_id) via
    // ux_bank_items_account_slot (see 0015_item_slot_cas_support.sql) --
    // bank_items didn't have a natural conflict target before that.
    bool SaveItemSlot(IDatabase& db, std::int64_t bankAccountId, std::uint32_t slotId,
                       const std::optional<Item>& expectedPrevious, const Item& item);
    bool ClearItemSlot(IDatabase& db, std::int64_t bankAccountId, std::uint32_t slotId,
                        const std::optional<Item>& expectedPrevious);
} // namespace BankRepository
