#include "BankRepository.h"

#include "storage/IDatabase.h"

namespace BankRepository
{
std::optional<Account> FindAccount(IDatabase& db, std::int64_t accountId)
{
    auto stmt = db.Prepare(
        "SELECT id, password, money FROM bank_accounts WHERE account_id = ? AND server_id = ?");
    stmt->Bind(0, accountId);
    stmt->Bind(1, kServerId);

    if (!stmt->Step())
        return std::nullopt;

    return Account{
        .id = std::get<int64_t>(stmt->Column(0)),
        .password = std::get<std::string>(stmt->Column(1)),
        .money = std::get<int64_t>(stmt->Column(2)),
    };
}

void SaveMoney(IDatabase& db, std::int64_t bankAccountId, std::int64_t money)
{
    auto stmt = db.Prepare("UPDATE bank_accounts SET money = ? WHERE id = ?");
    stmt->Bind(0, money);
    stmt->Bind(1, bankAccountId);
    stmt->Step();
}

void SavePassword(IDatabase& db, std::int64_t bankAccountId, const std::string& password)
{
    auto stmt = db.Prepare("UPDATE bank_accounts SET password = ? WHERE id = ?");
    stmt->Bind(0, password);
    stmt->Bind(1, bankAccountId);
    stmt->Step();
}

std::vector<SlotItem> LoadAllItems(IDatabase& db, std::int64_t bankAccountId)
{
    std::vector<SlotItem> result;

    auto stmt = db.Prepare("SELECT slot_id, item_id, quantity, refine_level, option_bits FROM "
                            "bank_items WHERE bank_accounts_id = ?");
    stmt->Bind(0, bankAccountId);

    while (stmt->Step())
    {
        const SqlValue quantityColumn = stmt->Column(2);
        const SqlValue refineLevelColumn = stmt->Column(3);
        const bool hasRefineLevel = std::holds_alternative<int64_t>(refineLevelColumn);

        result.push_back(SlotItem{
            .slot_id = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(0))),
            .item =
                Item{
                    .item_id = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(1))),
                    .quantity = std::holds_alternative<int64_t>(quantityColumn)
                                    ? static_cast<std::uint32_t>(std::get<int64_t>(quantityColumn))
                                    : 0,
                    .refine_level = hasRefineLevel
                                        ? static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn))
                                        : 0,
                    .has_refine_level = hasRefineLevel,
                    // bank_items has no item_level column -- not persisted.
                    .item_level = 0,
                    .option_bits = static_cast<std::uint64_t>(std::get<int64_t>(stmt->Column(4))),
                },
        });
    }

    return result;
}

void SaveItemSlot(IDatabase& db, std::int64_t bankAccountId, std::uint32_t slotId, const Item& item)
{
    auto findExisting =
        db.Prepare("SELECT id FROM bank_items WHERE bank_accounts_id = ? AND slot_id = ?");
    findExisting->Bind(0, bankAccountId);
    findExisting->Bind(1, static_cast<int64_t>(slotId));

    const SqlValue quantityValue =
        item.has_refine_level ? SqlValue{} : SqlValue{static_cast<int64_t>(item.quantity)};
    const SqlValue refineLevelValue =
        item.has_refine_level ? SqlValue{static_cast<int64_t>(item.refine_level)} : SqlValue{};

    if (findExisting->Step())
    {
        const int64_t rowId = std::get<int64_t>(findExisting->Column(0));

        auto update = db.Prepare("UPDATE bank_items SET item_id = ?, quantity = ?, refine_level = ?, "
                                  "option_bits = ? WHERE id = ?");
        update->Bind(0, static_cast<int64_t>(item.item_id));
        update->Bind(1, quantityValue);
        update->Bind(2, refineLevelValue);
        update->Bind(3, static_cast<int64_t>(item.option_bits));
        update->Bind(4, rowId);
        update->Step();
        return;
    }

    auto insert = db.Prepare("INSERT INTO bank_items (bank_accounts_id, slot_id, item_id, quantity, "
                              "refine_level, option_bits) VALUES (?, ?, ?, ?, ?, ?)");
    insert->Bind(0, bankAccountId);
    insert->Bind(1, static_cast<int64_t>(slotId));
    insert->Bind(2, static_cast<int64_t>(item.item_id));
    insert->Bind(3, quantityValue);
    insert->Bind(4, refineLevelValue);
    insert->Bind(5, static_cast<int64_t>(item.option_bits));
    insert->Step();
}

void ClearItemSlot(IDatabase& db, std::int64_t bankAccountId, std::uint32_t slotId)
{
    auto stmt = db.Prepare("DELETE FROM bank_items WHERE bank_accounts_id = ? AND slot_id = ?");
    stmt->Bind(0, bankAccountId);
    stmt->Bind(1, static_cast<int64_t>(slotId));
    stmt->Step();
}
} // namespace BankRepository
