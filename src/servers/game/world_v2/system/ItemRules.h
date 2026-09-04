#pragma once

#include "../component/Grid.h"
#include "../component/Items.h"
#include "../core/Entity.h"
#include "../event/ItemEvents.h"
#include "../world/MapWorld.h"

#include <cstdint>

namespace world_v2
{

// Puts an item on the ground and announces it.
//
// Nothing special is needed to keep it from blocking anyone, because
// nothing blocks anyone: an item goes into the tile index like everything
// else, several can share a square with each other and with whoever is
// standing there, and a creature can walk over one freely.
//
// Returns kNullEntity if (x, y) is off the map. Unwalkable terrain is fine;
// an item dropped against a wall is a normal thing for a corpse to leave
// behind, and only walking there is forbidden.
inline Entity SpawnGroundItem(MapWorld& world, std::uint32_t itemId, std::uint32_t quantity, std::uint32_t maxStack,
                              int x, int y)
{
    if (quantity == 0)
    {
        // A pile of nothing would read as already-claimed to PickupSystem
        // and sit on the map forever.
        return kNullEntity;
    }

    const Entity item = world.Spawn(x, y);
    if (item == kNullEntity)
    {
        return kNullEntity;
    }

    world.registry.Assign<GroundItemComponent>(item, itemId, quantity, maxStack);
    world.events.Emit(GroundItemSpawnedEvent{item, itemId, quantity, x, y});
    return item;
}

// The default resolution of what PickupSystem announces.
//
// Game policy on top of the framework, in the same shape as CombatRules and
// SpawnRules: the system decides *that* an item changed hands, this decides
// what becomes of the husk it left behind.
//
// Removing the entity is structural, which is why it happens here at the
// barrier rather than inside the sweep -- the item is never the entity the
// pickup view is visiting, so despawning it there would be reordering a
// pool out from under an iteration.
inline void InstallItemRules(MapWorld& world)
{
    world.events.Listen<ItemPickedUpEvent>(
        [&world](const ItemPickedUpEvent& event)
        {
            // An earlier handler in the same flush may already have removed
            // it.
            if (!world.registry.Exists(event.item))
            {
                return;
            }

            world.Despawn(event.item);
        });
}

} // namespace world_v2
