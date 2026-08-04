#pragma once

#include "Item.h"

#include <vector>

// A single map's ground-item state. Deliberately minimal for now -- just
// the collection itself; spawn/lookup/removal logic (e.g. wiring
// CG_ITEM_PICKUP to actually resolve and remove a ground item, rather than
// its current placeholder behavior) lands here as the simulation grows.
class Map
{
public:
    void AddItem(Item item);

    // Removes the ground item with this instance id (see Item::id). Returns
    // false if no such item was present (e.g. already picked up).
    bool RemoveItem(std::uint32_t id);

    const std::vector<Item>& Items() const;

private:
    std::vector<Item> m_items;
};
