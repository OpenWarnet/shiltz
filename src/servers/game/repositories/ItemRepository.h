#pragma once

#include "world/Item.h"
#include "world/Character.h" // CharacterEquipmentItem / CharacterInventoryItem

#include <cstdint>
#include <optional>
#include <vector>

class IDatabase;

// Owns every write against the equipment_slot and inventory_slot tables --
// the one place item DB access happens, so a stored item's item_level/
// option_bits can't drift between callers the way a handler-local raw-SQL
// path and Character's slot methods used to: dropping and picking an appraised
// item back up used to silently erase its rolled option_bits.
//
// No per-slot reads here -- session->character's equipment/inventory is kept
// in step with every write below (see Character::SetItemSlot/ClearItemSlot),
// so callers read that cache instead of round-tripping through a SELECT.
// The one read still owned here is the full-character load that seeds that
// cache in the first place (LoadAllEquipment/LoadAllInventory).
//
// Every Save*/Clear* below is a compare-and-swap: `expectedPrevious` is
// what the caller's cache currently believes is in the slot (nullopt =
// believes it's empty), and the write only applies if the DB's actual
// current content still matches that. Returns false (no write applied) on a
// mismatch -- meaning some other write for this same character landed
// between the caller's snapshot and this call (see the pipelined-request
// race described in GameSessionStore.h), and the caller must treat this as
// a conflict/rejection, not assume the write happened. Both tables keep a
// row rather than deleting it once cleared (item_id = NULL means empty),
// so a CAS attempt can always tell "already emptied by someone else" apart
// from "genuinely never touched" -- see 0015_item_slot_cas_support.sql.
namespace ItemRepository
{
    bool SaveEquipmentSlot(IDatabase& db, std::int64_t characterId, std::uint32_t slot,
                            const std::optional<Item>& expectedPrevious, const Item& item);
    bool ClearEquipmentSlot(IDatabase& db, std::int64_t characterId, std::uint32_t slot,
                             const std::optional<Item>& expectedPrevious);

    bool SaveInventorySlot(IDatabase& db, std::int64_t characterId, std::uint32_t slotIndex,
                            const std::optional<Item>& expectedPrevious, const Item& item);
    bool ClearInventorySlot(IDatabase& db, std::int64_t characterId, std::uint32_t slotIndex,
                             const std::optional<Item>& expectedPrevious);

    // Wire slots 0-12 are equipment, 13+ are inventory (see
    // InventoryItemList::kBagStartSlot) -- each lives in its own DB table;
    // these resolve that split so callers never branch on which table a
    // wire slot belongs to.
    bool SaveItemSlot(IDatabase& db, std::int64_t characterId, std::uint32_t wireSlotId,
                       const std::optional<Item>& expectedPrevious, const Item& item);
    bool ClearItemSlot(IDatabase& db, std::int64_t characterId, std::uint32_t wireSlotId,
                        const std::optional<Item>& expectedPrevious);

    // Full equipment/inventory load, for Character::LoadFromDB.
    std::vector<CharacterEquipmentItem> LoadAllEquipment(IDatabase& db, std::int64_t characterId);
    std::vector<CharacterInventoryItem> LoadAllInventory(IDatabase& db, std::int64_t characterId);

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

    // Wire slot -> (table, table-relative index). Exposed so Character's
    // in-memory equipment/inventory mirrors can be kept in step with the
    // same DB calls above without re-deriving this split -- see
    // Character::SetItemSlot/ClearItemSlot.
    SlotRef ResolveSlotRef(std::uint32_t wireSlotId);
} // namespace ItemRepository
