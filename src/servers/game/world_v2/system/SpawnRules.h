#pragma once

#include "../component/Spawn.h"
#include "../core/Entity.h"
#include "../event/SpawnEvents.h"
#include "../world/MapWorld.h"
#include "SpawnSystem.h"

#include <cstdint>
#include <functional>
#include <utility>

namespace world_v2
{

// Dresses a freshly placed monster in whatever `templateId` means.
//
// The split is the same one CombatRules draws: world_v2 knows how to place
// an entity and keep a population topped up, and knows nothing at all about
// what monster 1042 is. Health, faction, vision, attack power and drop
// table all come from .scr data the game layer owns, so it supplies this.
//
// Called with the entity already on the map and already linked to its
// spawner, so a factory only ever has to add components.
using MonsterFactory = std::function<void(MapWorld&, Entity monster, std::uint32_t templateId)>;

// Turns spawn requests into monsters, at the barrier.
//
// Placing an entity is structural, so it cannot happen where SpawnSystem
// runs. This is the other half: it picks the tile at the moment of
// placement -- when the claims made by requests ahead of it in the same
// flush are already real, so a burst filling an empty camp spreads out
// instead of every request choosing the same free tile.
//
// A request is dropped, not retried, when the spawner is gone or its area
// has no room. SpawnSystem recounts from scratch next tick and asks again,
// so nothing needs to be remembered in between.
inline void InstallSpawnRules(MapWorld& world, MonsterFactory factory)
{
    world.events.Listen<MonsterSpawnRequestEvent>(
        [&world, factory = std::move(factory)](const MonsterSpawnRequestEvent& event)
        {
            if (!world.registry.Exists(event.spawner))
            {
                return;
            }

            SpawnerComponent* definition = world.registry.TryGet<SpawnerComponent>(event.spawner);
            if (definition == nullptr)
            {
                return;
            }

            int x = 0;
            int y = 0;
            if (!SpawnSystem::FindFreeTile(world.tiles, *definition, x, y))
            {
                // The camp is full of its own monsters, or something has
                // been built over the spawn point.
                return;
            }

            const Entity monster = world.SpawnBlocking(x, y);
            if (monster == kNullEntity)
            {
                return;
            }

            // Advance only on a placement that actually happened, so a
            // blocked area does not silently churn through the sequence.
            ++definition->sequence;

            // The link SpawnSystem counts. Assigned here rather than left
            // to the factory, so a factory that forgets it cannot quietly
            // create a monster no spawner will ever replace.
            world.registry.Assign<SpawnedByComponent>(monster, event.spawner);

            if (factory)
            {
                factory(world, monster, event.templateId);
            }

            // Emitted after the factory, so a listener building a spawn
            // packet sees the finished monster rather than a bare position.
            world.events.Emit(MonsterSpawnedEvent{monster, event.spawner, event.templateId, x, y});
        });
}

} // namespace world_v2
