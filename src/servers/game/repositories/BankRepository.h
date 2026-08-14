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

    // Persists bank_accounts.money alone -- called by handlers/Store.cpp's
    // money-transfer handlers, which never touch bank_items.
    void SaveMoney(IDatabase& db, std::int64_t bankAccountId, std::int64_t money);

    // Persists bank_accounts.password alone -- called by
    // handlers/Store.cpp's HandleStorePwModify.
    void SavePassword(IDatabase& db, std::int64_t bankAccountId, const std::string& password);

    std::vector<SlotItem> LoadAllItems(IDatabase& db, std::int64_t bankAccountId);

    // Upserts by (bank_accounts_id, slot_id) -- bank_items has no unique
    // index on that pair, so this reads the row first rather than relying
    // on an ON CONFLICT upsert (see ItemRepository::SaveInventorySlot for
    // the table that does have one).
    void SaveItemSlot(IDatabase& db, std::int64_t bankAccountId, std::uint32_t slotId, const Item& item);
    void ClearItemSlot(IDatabase& db, std::int64_t bankAccountId, std::uint32_t slotId);
} // namespace BankRepository
