#pragma once

#include <cstdint>

class PayloadReader;

// CG_ITEM_PICKUP (wire code 64583, c2s) -- request to pick up a dropped
// item instance off the ground, into a specific inventory slot.
struct ItemPickup
{
    // Ground item instance id (see ItemMapNew.id) -- identifies *which*
    // dropped item this is, not its item type. Resolving the real item_id
    // requires looking this up in the map simulation's ground-item
    // registry, which doesn't exist yet -- TODO, deferred.
    std::uint32_t id = 0;
    // Target inventory slot, chosen client-side. Bag-relative -- same
    // numbering as inventory_slot.slot_index, NOT offset by
    // InventoryItemList::kBagStartSlot. The server trusts this as-is
    // rather than picking a slot itself.
    std::uint32_t slot_id = 0;

    bool Deserialize(PayloadReader& reader);
};
