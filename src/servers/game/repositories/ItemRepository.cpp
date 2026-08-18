#include "ItemRepository.h"

#include "protocol/server/InventoryItemList.h"
#include "storage/IDatabase.h"

namespace ItemRepository
{
namespace
{
// NULL for an empty expected slot -- matches how a real occupied row would
// bind its own has_refine_level-gated quantity/refine_level (see
// SaveInventorySlot/SaveEquipmentSlot's own item.has_refine_level Bind
// calls below).
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

// item_level/option_bits are NOT NULL columns (default 0 when empty), so
// the guard compares against 0 rather than NULL for an empty expected slot.
SqlValue ExpectedItemLevel(const std::optional<Item>& expected)
{
    return SqlValue{static_cast<int64_t>(expected ? expected->item_level : 0)};
}

SqlValue ExpectedOptionBits(const std::optional<Item>& expected)
{
    return SqlValue{static_cast<int64_t>(expected ? expected->option_bits : 0)};
}
} // namespace

bool SaveEquipmentSlot(IDatabase& db, std::int64_t characterId, std::uint32_t slot,
                        const std::optional<Item>& expectedPrevious, const Item& item)
{
    // UPSERT: the target row may not exist yet (equipment_slot rows aren't
    // pre-seeded per slot), or may already exist with a NULL item_id
    // (explicitly-empty convention -- see 0002_add_characters.sql). The
    // DO UPDATE's WHERE guard is only evaluated once a conflicting row
    // exists, so a genuinely first-ever write to a virgin slot always
    // succeeds via the INSERT branch regardless of what expectedPrevious
    // said -- see ItemRepository.h's compare-and-swap note.
    auto stmt = db.Prepare(
        "INSERT INTO equipment_slot (character_id, slot, item_id, refine_level, item_level, "
        "option_bits) VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(character_id, slot) DO UPDATE SET item_id = excluded.item_id, "
        "refine_level = excluded.refine_level, item_level = excluded.item_level, "
        "option_bits = excluded.option_bits "
        "WHERE equipment_slot.item_id IS ? AND equipment_slot.refine_level IS ? "
        "AND equipment_slot.item_level IS ? AND equipment_slot.option_bits IS ? "
        "RETURNING item_id");
    stmt->Bind(0, characterId);
    stmt->Bind(1, static_cast<int64_t>(slot));
    stmt->Bind(2, static_cast<int64_t>(item.item_id));
    stmt->Bind(3, item.has_refine_level ? SqlValue{static_cast<int64_t>(item.refine_level)}
                                        : SqlValue{});
    stmt->Bind(4, static_cast<int64_t>(item.item_level));
    stmt->Bind(5, static_cast<int64_t>(item.option_bits));
    stmt->Bind(6, ExpectedItemId(expectedPrevious));
    stmt->Bind(7, ExpectedRefineLevel(expectedPrevious));
    stmt->Bind(8, ExpectedItemLevel(expectedPrevious));
    stmt->Bind(9, ExpectedOptionBits(expectedPrevious));
    return stmt->Step();
}

bool ClearEquipmentSlot(IDatabase& db, std::int64_t characterId, std::uint32_t slot,
                         const std::optional<Item>& expectedPrevious)
{
    auto stmt = db.Prepare(
        "UPDATE equipment_slot SET item_id = NULL, refine_level = NULL, item_level = 0, "
        "option_bits = 0 "
        "WHERE character_id = ? AND slot = ? AND item_id IS ? AND refine_level IS ? "
        "AND item_level IS ? AND option_bits IS ? "
        "RETURNING slot");
    stmt->Bind(0, characterId);
    stmt->Bind(1, static_cast<int64_t>(slot));
    stmt->Bind(2, ExpectedItemId(expectedPrevious));
    stmt->Bind(3, ExpectedRefineLevel(expectedPrevious));
    stmt->Bind(4, ExpectedItemLevel(expectedPrevious));
    stmt->Bind(5, ExpectedOptionBits(expectedPrevious));
    return stmt->Step();
}

bool SaveInventorySlot(IDatabase& db, std::int64_t characterId, std::uint32_t slotIndex,
                        const std::optional<Item>& expectedPrevious, const Item& item)
{
    // UPSERT: dest slot_index may have no row yet (content can arrive from
    // equipment_slot, a different table, via SaveItemSlot). See
    // SaveEquipmentSlot above for why a virgin slot's INSERT branch ignores
    // expectedPrevious.
    auto stmt = db.Prepare(
        "INSERT INTO inventory_slot (character_id, slot_index, item_id, quantity, refine_level, "
        "item_level, option_bits) "
        "VALUES (?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(character_id, slot_index) DO UPDATE SET item_id = excluded.item_id, "
        "quantity = excluded.quantity, refine_level = excluded.refine_level, "
        "item_level = excluded.item_level, option_bits = excluded.option_bits "
        "WHERE inventory_slot.item_id IS ? AND inventory_slot.quantity IS ? "
        "AND inventory_slot.refine_level IS ? AND inventory_slot.item_level IS ? "
        "AND inventory_slot.option_bits IS ? "
        "RETURNING item_id");
    stmt->Bind(0, characterId);
    stmt->Bind(1, static_cast<int64_t>(slotIndex));
    stmt->Bind(2, static_cast<int64_t>(item.item_id));
    // Write the unused side as NULL so has_refine_level round-trips.
    stmt->Bind(3,
               item.has_refine_level ? SqlValue{} : SqlValue{static_cast<int64_t>(item.quantity)});
    stmt->Bind(4, item.has_refine_level ? SqlValue{static_cast<int64_t>(item.refine_level)}
                                        : SqlValue{});
    stmt->Bind(5, static_cast<int64_t>(item.item_level));
    stmt->Bind(6, static_cast<int64_t>(item.option_bits));
    stmt->Bind(7, ExpectedItemId(expectedPrevious));
    stmt->Bind(8, ExpectedQuantity(expectedPrevious));
    stmt->Bind(9, ExpectedRefineLevel(expectedPrevious));
    stmt->Bind(10, ExpectedItemLevel(expectedPrevious));
    stmt->Bind(11, ExpectedOptionBits(expectedPrevious));
    return stmt->Step();
}

bool ClearInventorySlot(IDatabase& db, std::int64_t characterId, std::uint32_t slotIndex,
                         const std::optional<Item>& expectedPrevious)
{
    // Row stays and item_id goes to NULL rather than being deleted -- see
    // ItemRepository.h's compare-and-swap note and
    // 0015_item_slot_cas_support.sql.
    auto stmt = db.Prepare(
        "UPDATE inventory_slot SET item_id = NULL, quantity = NULL, refine_level = NULL, "
        "item_level = 0, option_bits = 0 "
        "WHERE character_id = ? AND slot_index = ? AND item_id IS ? AND quantity IS ? "
        "AND refine_level IS ? AND item_level IS ? AND option_bits IS ? "
        "RETURNING slot_index");
    stmt->Bind(0, characterId);
    stmt->Bind(1, static_cast<int64_t>(slotIndex));
    stmt->Bind(2, ExpectedItemId(expectedPrevious));
    stmt->Bind(3, ExpectedQuantity(expectedPrevious));
    stmt->Bind(4, ExpectedRefineLevel(expectedPrevious));
    stmt->Bind(5, ExpectedItemLevel(expectedPrevious));
    stmt->Bind(6, ExpectedOptionBits(expectedPrevious));
    return stmt->Step();
}

SlotRef ResolveSlotRef(std::uint32_t wireSlotId)
{
    if (wireSlotId < InventoryItemList::kBagStartSlot)
        return SlotRef{.kind = SlotKind::Equipment, .index = wireSlotId};

    return SlotRef{
        .kind = SlotKind::Inventory,
        .index = wireSlotId - static_cast<std::uint32_t>(InventoryItemList::kBagStartSlot),
    };
}

bool SaveItemSlot(IDatabase& db, std::int64_t characterId, std::uint32_t wireSlotId,
                   const std::optional<Item>& expectedPrevious, const Item& item)
{
    const SlotRef ref = ResolveSlotRef(wireSlotId);

    if (ref.kind == SlotKind::Equipment)
        return SaveEquipmentSlot(db, characterId, ref.index, expectedPrevious, item);

    return SaveInventorySlot(db, characterId, ref.index, expectedPrevious, item);
}

bool ClearItemSlot(IDatabase& db, std::int64_t characterId, std::uint32_t wireSlotId,
                    const std::optional<Item>& expectedPrevious)
{
    const SlotRef ref = ResolveSlotRef(wireSlotId);

    if (ref.kind == SlotKind::Equipment)
        return ClearEquipmentSlot(db, characterId, ref.index, expectedPrevious);

    return ClearInventorySlot(db, characterId, ref.index, expectedPrevious);
}

std::vector<PlayerEquipmentItem> LoadAllEquipment(IDatabase& db, std::int64_t characterId)
{
    std::vector<PlayerEquipmentItem> result;

    auto stmt = db.Prepare(
        "SELECT slot, item_id, refine_level, item_level, option_bits FROM equipment_slot "
        "WHERE character_id = ?");
    stmt->Bind(0, characterId);

    while (stmt->Step())
    {
        const SqlValue itemIdColumn = stmt->Column(1);
        if (!std::holds_alternative<int64_t>(itemIdColumn))
            continue; // NULL item_id -- empty slot

        const SqlValue refineLevelColumn = stmt->Column(2);
        const bool hasRefineLevel = std::holds_alternative<int64_t>(refineLevelColumn);

        result.push_back(PlayerEquipmentItem{
            .slot = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(0))),
            .item =
                Item{
                    .item_id = static_cast<std::uint32_t>(std::get<int64_t>(itemIdColumn)),
                    .quantity = 0,
                    .refine_level = hasRefineLevel ? static_cast<std::uint32_t>(
                                                         std::get<int64_t>(refineLevelColumn))
                                                   : 0,
                    .has_refine_level = hasRefineLevel,
                    .item_level = static_cast<std::int32_t>(std::get<int64_t>(stmt->Column(3))),
                    .option_bits = static_cast<std::uint64_t>(std::get<int64_t>(stmt->Column(4))),
                },
        });
    }

    return result;
}

std::vector<PlayerInventoryItem> LoadAllInventory(IDatabase& db, std::int64_t characterId)
{
    std::vector<PlayerInventoryItem> result;

    auto stmt = db.Prepare(
        "SELECT slot_index, item_id, quantity, refine_level, item_level, option_bits FROM "
        "inventory_slot WHERE character_id = ?");
    stmt->Bind(0, characterId);

    while (stmt->Step())
    {
        const SqlValue itemIdColumn = stmt->Column(1);
        if (!std::holds_alternative<int64_t>(itemIdColumn))
            continue; // NULL item_id -- empty slot (row kept for CAS purposes)

        const SqlValue quantityColumn = stmt->Column(2);
        const SqlValue refineLevelColumn = stmt->Column(3);
        const bool hasRefineLevel = std::holds_alternative<int64_t>(refineLevelColumn);

        result.push_back(PlayerInventoryItem{
            .slot_index = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(0))),
            .item =
                Item{
                    .item_id = static_cast<std::uint32_t>(std::get<int64_t>(itemIdColumn)),
                    .quantity = std::holds_alternative<int64_t>(quantityColumn)
                                    ? static_cast<std::uint32_t>(std::get<int64_t>(quantityColumn))
                                    : 0,
                    .refine_level = hasRefineLevel ? static_cast<std::uint32_t>(
                                                         std::get<int64_t>(refineLevelColumn))
                                                   : 0,
                    .has_refine_level = hasRefineLevel,
                    .item_level = static_cast<std::int32_t>(std::get<int64_t>(stmt->Column(4))),
                    .option_bits = static_cast<std::uint64_t>(std::get<int64_t>(stmt->Column(5))),
                },
        });
    }

    return result;
}
} // namespace ItemRepository
