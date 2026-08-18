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

Account CreateAccount(IDatabase& db, std::int64_t accountId, const std::string& password)
{
    auto stmt = db.Prepare(
        "INSERT INTO bank_accounts (account_id, server_id, password, money) VALUES (?, ?, ?, 0)");
    stmt->Bind(0, accountId);
    stmt->Bind(1, kServerId);
    stmt->Bind(2, password);
    stmt->Step();

    return Account{
        .id = db.LastInsertRowId(),
        .password = password,
        .money = 0,
    };
}

std::optional<std::int64_t> TrySpendMoney(IDatabase& db, std::int64_t bankAccountId,
                                           std::int64_t amount)
{
    auto stmt = db.Prepare(
        "UPDATE bank_accounts SET money = money - ? WHERE id = ? AND money >= ? RETURNING money");
    stmt->Bind(0, amount);
    stmt->Bind(1, bankAccountId);
    stmt->Bind(2, amount);

    if (!stmt->Step())
        return std::nullopt;

    return std::get<int64_t>(stmt->Column(0));
}

std::int64_t AddMoney(IDatabase& db, std::int64_t bankAccountId, std::int64_t amount)
{
    auto stmt =
        db.Prepare("UPDATE bank_accounts SET money = money + ? WHERE id = ? RETURNING money");
    stmt->Bind(0, amount);
    stmt->Bind(1, bankAccountId);
    stmt->Step();
    return std::get<int64_t>(stmt->Column(0));
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

namespace
{
// Same convention as ItemRepository.cpp's Expected* helpers -- NULL for an
// empty expected slot.
SqlValue ExpectedItemId(const std::optional<Item>& expected)
{
    return expected ? SqlValue{static_cast<int64_t>(expected->item_id)} : SqlValue{};
}

SqlValue ExpectedQuantity(const std::optional<Item>& expected)
{
    if (!expected || expected->has_refine_level)
        return SqlValue{};
    return SqlValue{static_cast<int64_t>(expected->quantity)};
}

SqlValue ExpectedRefineLevel(const std::optional<Item>& expected)
{
    if (!expected || !expected->has_refine_level)
        return SqlValue{};
    return SqlValue{static_cast<int64_t>(expected->refine_level)};
}

// option_bits is a NOT NULL column (default 0 when empty), so the guard
// compares against 0 rather than NULL for an empty expected slot.
SqlValue ExpectedOptionBits(const std::optional<Item>& expected)
{
    return SqlValue{static_cast<int64_t>(expected ? expected->option_bits : 0)};
}
} // namespace

bool SaveItemSlot(IDatabase& db, std::int64_t bankAccountId, std::uint32_t slotId,
                   const std::optional<Item>& expectedPrevious, const Item& item)
{
    // Upserts via ux_bank_items_account_slot -- see BankRepository.h.
    auto stmt = db.Prepare(
        "INSERT INTO bank_items (bank_accounts_id, slot_id, item_id, quantity, refine_level, "
        "option_bits) VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(bank_accounts_id, slot_id) DO UPDATE SET item_id = excluded.item_id, "
        "quantity = excluded.quantity, refine_level = excluded.refine_level, "
        "option_bits = excluded.option_bits "
        "WHERE bank_items.item_id IS ? AND bank_items.quantity IS ? "
        "AND bank_items.refine_level IS ? AND bank_items.option_bits IS ? "
        "RETURNING item_id");
    stmt->Bind(0, bankAccountId);
    stmt->Bind(1, static_cast<int64_t>(slotId));
    stmt->Bind(2, static_cast<int64_t>(item.item_id));
    stmt->Bind(3,
               item.has_refine_level ? SqlValue{} : SqlValue{static_cast<int64_t>(item.quantity)});
    stmt->Bind(4, item.has_refine_level ? SqlValue{static_cast<int64_t>(item.refine_level)}
                                        : SqlValue{});
    stmt->Bind(5, static_cast<int64_t>(item.option_bits));
    stmt->Bind(6, ExpectedItemId(expectedPrevious));
    stmt->Bind(7, ExpectedQuantity(expectedPrevious));
    stmt->Bind(8, ExpectedRefineLevel(expectedPrevious));
    stmt->Bind(9, ExpectedOptionBits(expectedPrevious));
    return stmt->Step();
}

bool ClearItemSlot(IDatabase& db, std::int64_t bankAccountId, std::uint32_t slotId,
                    const std::optional<Item>& expectedPrevious)
{
    // Row stays and item_id goes to NULL rather than being deleted -- same
    // reasoning as ItemRepository::ClearInventorySlot.
    auto stmt = db.Prepare(
        "UPDATE bank_items SET item_id = NULL, quantity = NULL, refine_level = NULL, "
        "option_bits = 0 "
        "WHERE bank_accounts_id = ? AND slot_id = ? AND item_id IS ? AND quantity IS ? "
        "AND refine_level IS ? AND option_bits IS ? "
        "RETURNING slot_id");
    stmt->Bind(0, bankAccountId);
    stmt->Bind(1, static_cast<int64_t>(slotId));
    stmt->Bind(2, ExpectedItemId(expectedPrevious));
    stmt->Bind(3, ExpectedQuantity(expectedPrevious));
    stmt->Bind(4, ExpectedRefineLevel(expectedPrevious));
    stmt->Bind(5, ExpectedOptionBits(expectedPrevious));
    return stmt->Step();
}
} // namespace BankRepository
