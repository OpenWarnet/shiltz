#pragma once

#include "../Simulation.h"
#include "../component/Spawn.h"
#include "../core/Entity.h"
#include "../core/Map.h"
#include "../core/Module.h"
#include "../event/SpawnEvents.h"
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
using MonsterFactory = std::function<void(Map&, Entity monster, std::uint32_t templateId)>;

// Turns spawn requests into monsters, at the barrier.
//
// Placing an entity is structural, so it cannot happen where SpawnSystem
// runs. This is the other half: it picks the tile at the moment of
// placement and advances the spawner's sequence as it goes, so a burst
// filling an empty camp spreads across the area instead of every request
// landing on the same square. Cosmetic now rather than necessary --
// monsters may share a tile, so no request is refused for want of room --
// but a camp that arrives in one neat pile still looks wrong.
//
// A request is dropped, not retried, when the spawner is gone or its area
// is entirely unwalkable. SpawnSystem recounts from scratch next tick and
// asks again, so nothing needs to be remembered in between.
//
// Priming happens in Start rather than in a PrimeSpawns() the caller has to
// remember. That used to be an unwritten three-step contract -- install the
// rules, place the spawner entities, then prime, in that order -- with
// nothing but WorldRunner's call sequence saying so, and priming early
// meant the requests found no listener and the map came up empty. Start
// runs after every module's Setup, so the listener above is guaranteed
// live, and after the caller has authored its spawners.
class SpawnRulesModule : public Module
{
public:
    explicit SpawnRulesModule(MonsterFactory factory)
        : m_factory(std::move(factory))
    {
    }

    const char* Name() const override
    {
        return "SpawnRules";
    }

    void Setup(ModuleContext& context) override
    {
        Map& world = context.World();

        context.Listen<MonsterSpawnRequestEvent>(
            [&world, factory = m_factory](const MonsterSpawnRequestEvent& event)
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
                if (!SpawnSystem::FindSpawnTile(world.tiles, *definition, x, y))
                {
                    // Every tile in the area is unwalkable -- something has
                    // been built over the spawn point, or it was placed
                    // inside the terrain. Crowding is no longer a reason to
                    // fail.
                    return;
                }

                const Entity monster = world.Spawn(x, y);
                if (monster == kNullEntity)
                {
                    return;
                }

                // Advance only on a placement that actually happened, so a
                // blocked area does not silently churn through the
                // sequence.
                ++definition->sequence;

                // The link SpawnSystem counts. Assigned here rather than
                // left to the factory, so a factory that forgets it cannot
                // quietly create a monster no spawner will ever replace.
                world.registry.Assign<SpawnedByComponent>(monster, event.spawner);

                if (factory)
                {
                    factory(world, monster, event.templateId);
                }

                // Emitted after the factory, so a listener building a spawn
                // packet sees the finished monster rather than a bare
                // position.
                world.events.Emit(MonsterSpawnedEvent{monster, event.spawner, event.templateId, x, y});
            });
    }

    // Fills every spawner to strength and resolves it immediately, so a
    // fresh map comes up populated instead of trickling in over
    // desiredCount respawn intervals.
    //
    // Reaches the SpawnSystem through the simulation rather than owning
    // one, because the system belongs to whichever module installed it --
    // CoreSimulationModule, normally. A simulation running these rules with
    // no SpawnSystem is a legitimate arrangement (spawners driven entirely
    // by a game layer's own events), so a missing one is skipped rather
    // than asserted.
    void Start(Simulation& simulation) override
    {
        SpawnSystem* spawn = simulation.FindSystem<SpawnSystem>();
        if (spawn == nullptr)
        {
            return;
        }

        Map& world = simulation.World();
        spawn->Prime(world.registry, world.events);
        world.events.Flush();
    }

private:
    MonsterFactory m_factory;
};

} // namespace world_v2
