#pragma once

#include "../component/Spawn.h"
#include "../core/Entity.h"
#include "../core/EventManager.h"
#include "../core/Registry.h"
#include "../event/SpawnEvents.h"
#include "../world/TileGrid.h"

#include <cstdint>

namespace world_v2
{

// Keeps each spawner's population topped up.
//
// Runs in the simulation stage and, like every other system there, changes
// nothing structural: it counts, it counts down, and it emits a request.
// The monster itself is created at the barrier -- see SpawnRules.h.
//
// Counting
// --------
// aliveCount is recomputed from scratch every tick by tallying live
// SpawnedByComponent links, rather than being decremented when something
// dies. That costs one sweep over spawned monsters per tick, which the
// benchmarks put well inside the noise, and buys a count that cannot drift:
// no ordering dependency on who handles DeathEvent first, no leak when a
// monster leaves by a route nobody thought about, and a wrong count repairs
// itself on the next tick instead of persisting forever.
//
// Timing
// ------
// The respawn clock only runs while a spawner is short, and firing it emits
// exactly one request. So a camp that is wiped refills one monster per
// interval rather than the whole pack reappearing at once. Filling a
// spawner from empty at world load would take desiredCount intervals, which
// is why Prime exists.
class SpawnSystem
{
public:
    void Update(Registry& registry, EventManager& events, float deltaSeconds)
    {
        Recount(registry);

        registry.view<SpawnerComponent>().Each(
            [&](Entity spawner, SpawnerComponent& definition)
            {
                if (definition.aliveCount >= definition.desiredCount)
                {
                    // At strength: hold the clock at full, so the next loss
                    // waits the whole interval rather than however much was
                    // left over from the last one.
                    definition.timer = definition.respawnDelay;
                    return;
                }

                definition.timer -= deltaSeconds;
                if (definition.timer > 0.0f)
                {
                    return;
                }

                definition.timer = definition.respawnDelay;
                events.Emit(MonsterSpawnRequestEvent{spawner, definition.templateId});
            });
    }

    // Fills every spawner to strength at once, for world load.
    //
    // Emits the whole shortfall rather than one request, so a fresh map
    // comes up populated instead of trickling in over desiredCount respawn
    // intervals. The requests still resolve one at a time at the barrier,
    // each claiming its own tile; any that cannot find a free one is simply
    // dropped, and Update tops the spawner back up afterwards.
    void Prime(Registry& registry, EventManager& events)
    {
        Recount(registry);

        registry.view<SpawnerComponent>().Each(
            [&](Entity spawner, SpawnerComponent& definition)
            {
                for (int i = definition.aliveCount; i < definition.desiredCount; ++i)
                {
                    events.Emit(MonsterSpawnRequestEvent{spawner, definition.templateId});
                }

                definition.timer = definition.respawnDelay;
            });
    }

    // Picks the tile a spawn should land on, or reports that the area is
    // full.
    //
    // Starts from a position derived from the spawner's sequence counter so
    // successive monsters spread out instead of stacking against one corner,
    // then walks the whole area from there until it finds a tile that is
    // both walkable and unoccupied. Deterministic in every part: the same
    // spawner in the same state always yields the same tile, which is what
    // keeps two runs of the same world identical.
    //
    // Returns false when the area has no free tile at all -- a camp packed
    // with its own monsters, or a spawn point that has been built over.
    static bool FindFreeTile(const TileGrid& tiles, const SpawnerComponent& definition, int& outX, int& outY)
    {
        const int span = definition.radius * 2 + 1;
        const int total = span * span;
        if (total <= 0)
        {
            return false;
        }

        const int start = static_cast<int>(Mix(definition.sequence, static_cast<std::uint32_t>(definition.templateId)) %
                                           static_cast<std::uint32_t>(total));

        for (int step = 0; step < total; ++step)
        {
            const int index = (start + step) % total;
            const int x = definition.x - definition.radius + (index % span);
            const int y = definition.y - definition.radius + (index / span);

            if (tiles.IsFree(x, y))
            {
                outX = x;
                outY = y;
                return true;
            }
        }

        return false;
    }

private:
    // Zeroes every spawner, then tallies one for each living monster that
    // still points at one.
    static void Recount(Registry& registry)
    {
        registry.view<SpawnerComponent>().Each([](Entity, SpawnerComponent& definition) { definition.aliveCount = 0; });

        registry.view<SpawnedByComponent>().Each(
            [&registry](Entity, SpawnedByComponent& link)
            {
                // A link to a spawner that has been destroyed counts for
                // nobody, rather than incrementing whatever took its slot.
                if (SpawnerComponent* definition = registry.TryGet<SpawnerComponent>(link.spawner))
                {
                    ++definition->aliveCount;
                }
            });
    }

    // A small integer hash. Not cryptographic and not trying to be -- it
    // only has to scatter consecutive sequence numbers across the spawn
    // area, and do it the same way every run.
    static std::uint32_t Mix(std::uint32_t a, std::uint32_t b)
    {
        std::uint32_t value = a * 0x9E3779B9u + b * 0x85EBCA6Bu;
        value ^= value >> 15;
        value *= 0x2545F491u;
        value ^= value >> 13;
        return value;
    }
};

} // namespace world_v2
