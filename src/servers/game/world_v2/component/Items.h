#pragma once

#include <cstdint>
#include <vector>

namespace world_v2
{

// Which drop table to roll when this entity dies. The table itself is
// static .scr data and lives outside the registry -- only the id is per
// entity.
struct LootTableComponent
{
    std::uint32_t dropTableId = 0;
};

// An item lying on the ground, waiting to be picked up.
//
// An ordinary entity with a GridPositionComponent, standing on a tile like
// everything else. It blocks nobody, because nothing blocks anybody, and it
// is not a target because it has no FactionComponent -- see AISystem, which
// scans the same tiles and skips it for that reason.
//
// `maxStack` travels with the instance rather than being looked up, for the
// same reason a spawner carries a templateId: how many of item 1042 fit in
// one slot is .scr data, and world_v2 does not read .scr files. Whoever
// creates the item knows, and passes it along.
//
// A quantity of zero means claimed -- see PickupSystem, where it is what
// stops two players in the same tick from each walking away with a copy.
struct GroundItemComponent
{
    std::uint32_t itemId = 0;
    std::uint32_t quantity = 0;
    std::uint32_t maxStack = 1;
};

// What an entity is carrying.
//
// The one component here that is not a flat POD: it owns a heap allocation,
// so the pool moves rather than memcpys it. Everything else about the
// sparse set is unchanged -- the dense array just holds vectors.
//
// `slots` is fixed-length and always full-length: an inventory of twenty
// slots holds twenty entries for its whole life, and an unused one is a
// slot with quantity zero rather than a missing element. That keeps "which
// slot is this item in" a stable index -- which matters the moment slot
// numbers go on the wire -- instead of something that shifts every time an
// earlier stack is emptied.
//
// The operations that respect all of this live in world/Inventory.h. Adding
// to `slots` by hand will not.
struct InventoryComponent
{
    struct Slot
    {
        std::uint32_t itemId = 0;
        std::uint32_t quantity = 0;

        bool IsEmpty() const
        {
            return quantity == 0;
        }
    };

    std::vector<Slot> slots;
};

} // namespace world_v2
