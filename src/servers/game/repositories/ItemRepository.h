#pragma once

#include "world/Item.h"
#include "world/Player.h" // PlayerEquipmentItem / PlayerInventoryItem

#include <cstdint>
#include <vector>

class IDatabase;

// Owns every write against the equipment_slot and inventory_slot tables --
// the one place item DB access happens, so a stored item's item_level/
// option_bits can't drift between callers the way a handler-local raw-SQL
// path and Player's slot methods used to: dropping and picking an appraised
// item back up used to silently erase its rolled option_bits.
//
// No per-slot reads here -- session->player's equipment/inventory is kept
// in step with every write below (see Player::SetItemSlot/ClearItemSlot),
// so callers read that cache instead of round-tripping through a SELECT.
// The one read still owned here is the full-character load that seeds that
// cache in the first place (LoadAllEquipment/LoadAllInventory).
namespace ItemRepository
{
    void SaveEquipmentSlot(IDatabase& db, std::int64_t characterId, std::uint32_t slot, const Item& item);
    void ClearEquipmentSlot(IDatabase& db, std::int64_t characterId, std::uint32_t slot);

    void SaveInventorySlot(IDatabase& db, std::int64_t characterId, std::uint32_t slotIndex, const Item& item);
    void ClearInventorySlot(IDatabase& db, std::int64_t characterId, std::uint32_t slotIndex);

    // Wire slots 0-12 are equipment, 13+ are inventory (see
    // InventoryItemList::kBagStartSlot) -- each lives in its own DB table;
    // these resolve that split so callers never branch on which table a
    // wire slot belongs to.
    void SaveItemSlot(IDatabase& db, std::int64_t characterId, std::uint32_t wireSlotId, const Item& item);
    void ClearItemSlot(IDatabase& db, std::int64_t characterId, std::uint32_t wireSlotId);

    // Full equipment/inventory load, for Player::LoadFromDB.
    std::vector<PlayerEquipmentItem> LoadAllEquipment(IDatabase& db, std::int64_t characterId);
    std::vector<PlayerInventoryItem> LoadAllInventory(IDatabase& db, std::int64_t characterId);

    enum class SlotKind
    {
        Equipment,
        Inventory,
    };

    struct SlotRef
    {
        SlotKind kind;
        std::uint32_t index;
    };

    // Wire slot -> (table, table-relative index). Exposed so Player's
    // in-memory equipment/inventory mirrors can be kept in step with the
    // same DB calls above without re-deriving this split -- see
    // Player::SetItemSlot/ClearItemSlot.
    SlotRef ResolveSlotRef(std::uint32_t wireSlotId);
} // namespace ItemRepository
