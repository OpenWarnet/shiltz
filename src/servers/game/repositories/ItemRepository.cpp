#include "ItemRepository.h"

#include "protocol/server/InventoryItemList.h"
#include "storage/IDatabase.h"

namespace ItemRepository
{
    void SaveEquipmentSlot(IDatabase& db, std::int64_t characterId, std::uint32_t slot, const Item& item)
    {
        // UPSERT: the target row may not exist yet (equipment_slot rows
        // aren't pre-seeded per slot), or may already exist with a NULL
        // item_id (explicitly-empty convention -- see 0002_add_characters.sql).
        auto stmt = db.Prepare(
            "INSERT INTO equipment_slot (character_id, slot, item_id, refine_level, item_level, "
            "option_bits) VALUES (?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(character_id, slot) DO UPDATE SET item_id = excluded.item_id, "
            "refine_level = excluded.refine_level, item_level = excluded.item_level, "
            "option_bits = excluded.option_bits");
        stmt->Bind(0, characterId);
        stmt->Bind(1, static_cast<int64_t>(slot));
        stmt->Bind(2, static_cast<int64_t>(item.item_id));
        stmt->Bind(3, item.has_refine_level ? SqlValue{static_cast<int64_t>(item.refine_level)} : SqlValue{});
        stmt->Bind(4, static_cast<int64_t>(item.item_level));
        stmt->Bind(5, static_cast<int64_t>(item.option_bits));
        stmt->Step();
    }

    void ClearEquipmentSlot(IDatabase& db, std::int64_t characterId, std::uint32_t slot)
    {
        auto stmt = db.Prepare("UPDATE equipment_slot SET item_id = NULL, refine_level = NULL "
                                "WHERE character_id = ? AND slot = ?");
        stmt->Bind(0, characterId);
        stmt->Bind(1, static_cast<int64_t>(slot));
        stmt->Step();
    }

    void SaveInventorySlot(IDatabase& db, std::int64_t characterId, std::uint32_t slotIndex,
                            const Item& item)
    {
        // UPSERT: dest slot_index may have no row yet (content can arrive
        // from equipment_slot, a different table, via SaveItemSlot).
        auto stmt = db.Prepare(
            "INSERT INTO inventory_slot (character_id, slot_index, item_id, quantity, refine_level, "
            "item_level, option_bits) "
            "VALUES (?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(character_id, slot_index) DO UPDATE SET item_id = excluded.item_id, "
            "quantity = excluded.quantity, refine_level = excluded.refine_level, "
            "item_level = excluded.item_level, option_bits = excluded.option_bits");
        stmt->Bind(0, characterId);
        stmt->Bind(1, static_cast<int64_t>(slotIndex));
        stmt->Bind(2, static_cast<int64_t>(item.item_id));
        // Write the unused side as NULL so has_refine_level round-trips.
        stmt->Bind(3, item.has_refine_level ? SqlValue{} : SqlValue{static_cast<int64_t>(item.quantity)});
        stmt->Bind(4, item.has_refine_level ? SqlValue{static_cast<int64_t>(item.refine_level)} : SqlValue{});
        stmt->Bind(5, static_cast<int64_t>(item.item_level));
        stmt->Bind(6, static_cast<int64_t>(item.option_bits));
        stmt->Step();
    }

    void ClearInventorySlot(IDatabase& db, std::int64_t characterId, std::uint32_t slotIndex)
    {
        auto stmt = db.Prepare("DELETE FROM inventory_slot WHERE character_id = ? AND slot_index = ?");
        stmt->Bind(0, characterId);
        stmt->Bind(1, static_cast<int64_t>(slotIndex));
        stmt->Step();
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

    void SaveItemSlot(IDatabase& db, std::int64_t characterId, std::uint32_t wireSlotId, const Item& item)
    {
        const SlotRef ref = ResolveSlotRef(wireSlotId);

        if (ref.kind == SlotKind::Equipment)
        {
            SaveEquipmentSlot(db, characterId, ref.index, item);
        }
        else
        {
            SaveInventorySlot(db, characterId, ref.index, item);
        }
    }

    void ClearItemSlot(IDatabase& db, std::int64_t characterId, std::uint32_t wireSlotId)
    {
        const SlotRef ref = ResolveSlotRef(wireSlotId);

        if (ref.kind == SlotKind::Equipment)
        {
            ClearEquipmentSlot(db, characterId, ref.index);
        }
        else
        {
            ClearInventorySlot(db, characterId, ref.index);
        }
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
                .item = Item{
                    .item_id = static_cast<std::uint32_t>(std::get<int64_t>(itemIdColumn)),
                    .quantity = 0,
                    .refine_level = hasRefineLevel
                                        ? static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn))
                                        : 0,
                    .has_refine_level = hasRefineLevel,
                    .item_level = static_cast<std::int32_t>(std::get<int64_t>(stmt->Column(3))),
                    .option_bits = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(4))),
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
            const SqlValue quantityColumn = stmt->Column(2);
            const SqlValue refineLevelColumn = stmt->Column(3);
            const bool hasRefineLevel = std::holds_alternative<int64_t>(refineLevelColumn);

            result.push_back(PlayerInventoryItem{
                .slot_index = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(0))),
                .item = Item{
                    .item_id = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(1))),
                    .quantity = std::holds_alternative<int64_t>(quantityColumn)
                                    ? static_cast<std::uint32_t>(std::get<int64_t>(quantityColumn))
                                    : 0,
                    .refine_level = hasRefineLevel
                                        ? static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn))
                                        : 0,
                    .has_refine_level = hasRefineLevel,
                    .item_level = static_cast<std::int32_t>(std::get<int64_t>(stmt->Column(4))),
                    .option_bits = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(5))),
                },
            });
        }

        return result;
    }
} // namespace ItemRepository
