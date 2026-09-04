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

    // Creates an entity standing at (x, y), returning kNullEntity only if
    // the tile is off the map.
    //
    // One function, because there is nothing left for a second one to mean.
    // This used to be a pair -- SpawnBlocking for creatures, which claimed
    // the tile exclusively and refused if it was taken, and SpawnPassable
    // for items, which stayed out of the occupancy array so as not to block
    // anyone. With tiles holding any number of entities, the first refusal
    // is gone and the second distinction has no purpose: everything with a
    // position goes in the index, and anything that cares what kind of
    // thing it found asks the registry, as AI targeting already does.
    //
    // Terrain is not consulted here. Placement is authoring -- loot lands
    // where its corpse fell, wall or no wall -- while walking onto a tile
    // is gameplay, and GridMovementSystem is what enforces that. A caller
    // that wants a creature on walkable ground checks IsWalkable first;
    // SpawnRules does.
    //
    // This and Despawn are the chokepoint TileGrid's invariant depends on:
    // they add the entity to the tile's list and give it its
    // GridPositionComponent in one step, so the two cannot disagree.
    Entity Spawn(int x, int y)
    {
        if (!tiles.InBounds(x, y))
        {
            return kNullEntity;
        }

        const Entity entity = registry.Create();
        if (!tiles.Place(entity, x, y))
        {
            // Unreachable given the InBounds check above, but leaving a
            // half-spawned entity behind would be worse than a wasted slot.
            registry.Destroy(entity);
            return kNullEntity;
        }

        registry.Assign<GridPositionComponent>(entity, x, y);
        return entity;
    }

    // Takes the entity off its tile and destroys it.
    //
    // TileGrid::Remove takes only the named entity, so despawning one thing
    // never disturbs whatever else is standing on the same square.
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
    // the pieces they need by reference. The one rule is that a
    // GridPositionComponent is never assigned through `registry` directly;
    // Spawn, Despawn, and GridMovementSystem own that, and they are what
    // keep `tiles` honest.
    Registry registry;
    TileGrid tiles;
    EventManager events;
};

} // namespace world_v2
