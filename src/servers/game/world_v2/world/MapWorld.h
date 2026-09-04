#pragma once

#include "../component/Grid.h"
#include "../core/EventManager.h"
#include "../core/Registry.h"
#include "TileGrid.h"

namespace world_v2
{

// Everything one map is: its entities, its terrain, and its pending events.
//
// Maps do not interact. A monster on one cannot see, target, or path
// towards anything on another, so each gets its own Registry rather than
// sharing one global registry filtered by a map id. Three things fall out
// of that:
//
//   * every view is already scoped to one map, with no per-entity map
//     comparison in the inner loop
//   * a map's population bounds its own iteration cost -- a busy town does
//     not slow down an empty field
//   * two maps share no mutable state, so ticking them on separate threads
//     later needs no locking, only that a tick not overlap itself
//
// The trade is that an Entity is only meaningful inside its own MapWorld,
// and a warp is a despawn here plus a spawn there rather than a field
// write. That is honest about what a warp is: everyone watching the old map
// has to be told the entity left, and everyone on the new one that it
// arrived.
//
// Single-threaded, like the Registry inside it.
class MapWorld
{
public:
    MapWorld(int width, int height, bool walkableByDefault = false)
        : tiles(width, height, walkableByDefault)
    {
    }

    // Creates an entity that occupies (x, y), returning kNullEntity if the
    // tile is unwalkable or already taken. For creatures -- anything that
    // should block others and be findable by tile.
    //
    // This and Despawn are the chokepoint TileGrid's invariant depends on:
    // they place the entity in the occupancy array and give it its
    // GridPositionComponent in one step, so the two cannot disagree.
    Entity SpawnBlocking(int x, int y)
    {
        if (!tiles.IsFree(x, y))
        {
            return kNullEntity;
        }

        const Entity entity = registry.Create();
        if (!tiles.TryPlace(entity, x, y))
        {
            // Unreachable given the IsFree check above, but leaving a
            // half-spawned entity behind would be worse than a wasted slot.
            registry.Destroy(entity);
            return kNullEntity;
        }

        registry.Assign<GridPositionComponent>(entity, x, y);
        return entity;
    }

    // Creates an entity that sits at (x, y) without claiming it -- ground
    // items, effects, anything that should not block a creature or be
    // returned by OccupantAt. It still has a position; it is simply not in
    // the occupancy array.
    //
    // Fails only if (x, y) is off the map. Passable things are allowed on
    // unwalkable terrain: an item dropped against a wall is fine.
    Entity SpawnPassable(int x, int y)
    {
        if (!tiles.InBounds(x, y))
        {
            return kNullEntity;
        }

        const Entity entity = registry.Create();
        registry.Assign<GridPositionComponent>(entity, x, y);
        return entity;
    }

    // Releases the entity's tile, if it held one, and destroys it.
    //
    // Safe for passable entities too: TileGrid::Remove only clears a tile
    // whose occupant is actually this entity, so despawning an item never
    // evicts a creature standing on the same square.
    //
    // Structural -- call it at the barrier, not inside a system sweep,
    // except on the entity a view is currently visiting.
    void Despawn(Entity entity)
    {
        if (!registry.Exists(entity))
        {
            return;
        }

        if (const GridPositionComponent* position = registry.TryGet<GridPositionComponent>(entity))
        {
            tiles.Remove(entity, position->x, position->y);
        }

        registry.Destroy(entity);
    }

    // Public because this is a bundle, not an abstraction -- systems take
    // the pieces they need by reference. The one rule is that a blocking
    // entity's position is never assigned through `registry` directly;
    // SpawnBlocking, Despawn, and GridMovementSystem own that, and they are
    // what keep `tiles` honest.
    Registry registry;
    TileGrid tiles;
    EventManager events;
};

} // namespace world_v2
