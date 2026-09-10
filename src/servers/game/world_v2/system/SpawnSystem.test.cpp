#include "../Simulation.h"
#include "BroadcastModule.h"
#include "CoreSimulationModule.h"
#include "../component/Combat.h"
#include "../component/Grid.h"
#include "../component/Spawn.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "../event/SpawnEvents.h"
#include "CombatRules.h"
#include "SpawnRules.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace world_v2;

namespace
{

constexpr int kMonsterFaction = 1;
constexpr std::uint32_t kTemplate = 1042;
constexpr float kRespawnDelay = 1.0f;
constexpr float kStep = 0.25f;

// What the game layer would supply: everything world_v2 cannot know from a
// template id alone.
void DressMonster(Map& world, Entity monster, std::uint32_t templateId)
{
    world.registry.Assign<FactionComponent>(monster, kMonsterFaction);
    world.registry.Assign<HealthComponent>(monster, 10, 10);
    world.registry.Assign<ExperienceRewardComponent>(monster, static_cast<std::uint64_t>(templateId));
}

Entity AddSpawner(Simulation& simulation, int x, int y, int radius, int desiredCount)
{
    const Entity spawner = simulation.World().registry.Create();
    simulation.World().registry.Assign<SpawnerComponent>(spawner, kTemplate, x, y, radius, desiredCount, 0,
                                                         kRespawnDelay, 0.0f, std::uint32_t{0});
    return spawner;
}

std::vector<Entity> LiveMonsters(Map& world)
{
    std::vector<Entity> found;
    world.registry.view<SpawnedByComponent>().Each([&found](Entity entity, SpawnedByComponent&)
                                                   { found.push_back(entity); });
    std::sort(found.begin(), found.end());
    return found;
}

void PrimeFillsToStrength()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    const Entity spawner = AddSpawner(simulation, 20, 20, 3, 6);
    simulation.Start();

    const std::vector<Entity> monsters = LiveMonsters(simulation.World());
    CHECK_EQ(monsters.size(), 6u);

    // A fresh map should come up populated, not trickle in over six respawn
    // intervals.
    for (const Entity monster : monsters)
    {
        CHECK_EQ(simulation.World().registry.Get<SpawnedByComponent>(monster).spawner, spawner);

        // Placed inside the area, and dressed by the factory.
        const GridPositionComponent& position = simulation.World().registry.Get<GridPositionComponent>(monster);
        CHECK(position.x >= 17 && position.x <= 23);
        CHECK(position.y >= 17 && position.y <= 23);
        CHECK(simulation.World().registry.Has<HealthComponent>(monster));
        CHECK(simulation.World().registry.Has<FactionComponent>(monster));

        // On the map and in the tile index, like any other creature.
        CHECK(simulation.World().tiles.Contains(monster, position.x, position.y));
    }
}

void ABurstSpreadsAcrossItsArea()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    // Twenty monsters into a 5x5 area. Nothing stops them stacking now --
    // sharing a tile is legal -- so what is being checked is that the
    // sequence-driven tile choice still scatters them rather than dropping
    // the whole burst on one square, which would look like a bug even
    // though nothing would be broken.
    AddSpawner(simulation, 30, 30, 2, 20);
    simulation.Start();

    const std::vector<Entity> monsters = LiveMonsters(simulation.World());
    CHECK_EQ(monsters.size(), 20u);

    std::vector<int> tiles;
    for (const Entity monster : monsters)
    {
        const GridPositionComponent& position = simulation.World().registry.Get<GridPositionComponent>(monster);
        tiles.push_back(position.y * 64 + position.x);
    }

    std::sort(tiles.begin(), tiles.end());
    const std::size_t distinct =
        static_cast<std::size_t>(std::unique(tiles.begin(), tiles.end()) - tiles.begin());

    // The area holds twenty-five tiles and the chooser walks it in order
    // from a sequence-derived offset, so twenty spawns land on twenty
    // distinct squares. Asserted as a floor rather than exact equality:
    // spreading is a presentation choice, and a future chooser that
    // occasionally doubles up is not a defect the way an empty camp is.
    CHECK(distinct >= 15u);
}

void MonstersMayShareATile()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    // A 1x1 area asked for four monsters. Under exclusive occupancy this
    // was a camp that could never reach strength -- three requests dropped
    // every tick, forever. Now the single tile takes all four.
    AddSpawner(simulation, 30, 30, 0, 4);
    simulation.Start();

    CHECK_EQ(LiveMonsters(simulation.World()).size(), 4u);
    CHECK_EQ(simulation.World().tiles.OccupantCount(30, 30), std::size_t{4});

    // And it holds there rather than churning: a spawner at strength stops
    // asking.
    for (int tick = 0; tick < 20; ++tick)
    {
        simulation.Tick(0.5f);
    }
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 4u);
}

void ACrowdedAreaStillReachesStrength()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    // A 3x3 area asked for 20 monsters -- nine tiles, one of them walled.
    //
    // This used to be the test that a camp fills only what it can: eight
    // tiles meant eight monsters, and the other twelve requests were
    // dropped every tick forever. Since monsters share tiles, area no
    // longer caps population -- only walkability does, and eight walkable
    // tiles hold all twenty.
    AddSpawner(simulation, 30, 30, 1, 20);
    simulation.World().tiles.SetWalkable(30, 30, false);
    simulation.Start();

    CHECK_EQ(LiveMonsters(simulation.World()).size(), 20u);

    // The walled tile is still refused, though: terrain is what a spawn
    // cannot cross.
    CHECK(simulation.World().tiles.OccupantsAt(30, 30).empty());

    // At strength, so it holds rather than churning.
    for (int i = 0; i < 20; ++i)
    {
        simulation.Tick(kStep);
    }
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 20u);
}

void NoRoomAtAllIsHarmless()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    AddSpawner(simulation, 30, 30, 0, 4);
    simulation.World().tiles.SetWalkable(30, 30, false);
    simulation.Start();

    CHECK_EQ(LiveMonsters(simulation.World()).size(), 0u);

    simulation.Tick(kStep);
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 0u);
}

void ALossIsReplacedAfterTheDelay()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    AddSpawner(simulation, 20, 20, 3, 4);
    simulation.Start();
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 4u);

    simulation.World().Despawn(LiveMonsters(simulation.World()).front());
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 3u);

    // The shortfall is noticed on the next tick's recount, then the clock
    // runs. Nothing comes back early.
    simulation.Tick(kStep);
    simulation.Tick(kStep);
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 3u);

    for (int i = 0; i < 6; ++i)
    {
        simulation.Tick(kStep);
    }
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 4u);
}

void LossesComeBackOneAtATime()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    AddSpawner(simulation, 20, 20, 4, 6);
    simulation.Start();

    // Wipe the camp.
    for (const Entity monster : LiveMonsters(simulation.World()))
    {
        simulation.World().Despawn(monster);
    }
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 0u);

    // One respawn interval brings back one monster, not the whole pack --
    // clearing a camp should refill gradually.
    for (int i = 0; i < 6; ++i)
    {
        simulation.Tick(kStep);
    }
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 1u);

    for (int i = 0; i < 4; ++i)
    {
        simulation.Tick(kStep);
    }
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 2u);

    // And it does eventually get all the way back.
    for (int i = 0; i < 40; ++i)
    {
        simulation.Tick(kStep);
    }
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 6u);
}

void AFullSpawnerNeverOverfills()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    const Entity spawner = AddSpawner(simulation, 20, 20, 4, 3);
    simulation.Start();

    for (int i = 0; i < 100; ++i)
    {
        simulation.Tick(kStep);
    }

    CHECK_EQ(LiveMonsters(simulation.World()).size(), 3u);
    CHECK_EQ(simulation.World().registry.Get<SpawnerComponent>(spawner).aliveCount, 3);
}

void CountIsDerivedNotBookkept()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    const Entity spawner = AddSpawner(simulation, 20, 20, 4, 5);
    simulation.Start();

    simulation.Tick(kStep);
    CHECK_EQ(simulation.World().registry.Get<SpawnerComponent>(spawner).aliveCount, 5);

    // Removed by a route the spawner knows nothing about -- no DeathEvent,
    // no combat, just gone. A count maintained by decrementing on death
    // would now be wrong forever; a derived one repairs itself.
    const std::vector<Entity> monsters = LiveMonsters(simulation.World());
    simulation.World().Despawn(monsters[0]);
    simulation.World().Despawn(monsters[1]);

    simulation.Tick(kStep);
    CHECK_EQ(simulation.World().registry.Get<SpawnerComponent>(spawner).aliveCount, 3);
}

void MonstersOfADestroyedSpawnerCountForNobody()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    const Entity first = AddSpawner(simulation, 20, 20, 3, 3);
    simulation.Start();
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 3u);

    // The spawner goes away; its monsters stay, holding a stale link.
    simulation.World().registry.Destroy(first);

    // A new spawner takes over the freed slot. The orphans must not be
    // counted as its.
    const Entity second = AddSpawner(simulation, 40, 40, 3, 2);
    CHECK_EQ(EntityIndex(second), EntityIndex(first));

    for (int i = 0; i < 12; ++i)
    {
        simulation.Tick(kStep);
    }

    CHECK_EQ(simulation.World().registry.Get<SpawnerComponent>(second).aliveCount, 2);

    // Three orphans plus the new spawner's two.
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 5u);
}

void CombatDeathsAreReplaced()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);
    simulation.Install<CombatRulesModule>();

    AddSpawner(simulation, 20, 20, 3, 2);
    simulation.Start();

    const std::vector<Entity> monsters = LiveMonsters(simulation.World());
    CHECK_EQ(monsters.size(), 2u);

    // The real path: health hits zero, DeathSystem announces, the barrier
    // despawns, and the spawner notices on its next recount.
    simulation.World().registry.Get<HealthComponent>(monsters[0]).current = 0;

    simulation.Tick(kStep);
    CHECK(!simulation.World().registry.Exists(monsters[0]));
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 1u);

    for (int i = 0; i < 8; ++i)
    {
        simulation.Tick(kStep);
    }
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 2u);
}

void SpawnersAreNotOnTheMap()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    const Entity spawner = AddSpawner(simulation, 20, 20, 3, 2);
    simulation.Start();

    // A spawn point is bookkeeping, not a creature: nothing can walk into
    // it, target it, or see it.
    CHECK(!simulation.World().registry.Has<GridPositionComponent>(spawner));
    CHECK(!simulation.World().registry.Has<FactionComponent>(spawner));
    CHECK(!simulation.World().tiles.Contains(spawner, 20, 20));

    // Nor is it anywhere else on the index: a spawner has no position at
    // all, so no tile may list it.
    bool listedAnywhere = false;
    for (int y = 0; y < simulation.World().tiles.Height(); ++y)
    {
        for (int x = 0; x < simulation.World().tiles.Width(); ++x)
        {
            if (simulation.World().tiles.Contains(spawner, x, y))
            {
                listedAnywhere = true;
            }
        }
    }
    CHECK(!listedAnywhere);
}

void SpawnedEventDescribesAFinishedMonster()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();

    std::vector<MonsterSpawnedEvent> announced;
    bool dressedWhenAnnounced = true;
    simulation.World().events.Listen<MonsterSpawnedEvent>(
        [&](const MonsterSpawnedEvent& event)
        {
            announced.push_back(event);
            // Emitted after the factory ran, so a listener building a spawn
            // packet sees stats rather than a bare position.
            dressedWhenAnnounced =
                dressedWhenAnnounced && simulation.World().registry.Has<HealthComponent>(event.entity);
        });

    simulation.Install<SpawnRulesModule>(DressMonster);
    AddSpawner(simulation, 20, 20, 3, 3);
    simulation.Start();

    CHECK_EQ(announced.size(), 3u);
    CHECK(dressedWhenAnnounced);
    for (const MonsterSpawnedEvent& event : announced)
    {
        CHECK_EQ(event.templateId, kTemplate);
        CHECK_EQ(simulation.World().registry.Get<GridPositionComponent>(event.entity).x, event.x);
        CHECK_EQ(simulation.World().registry.Get<GridPositionComponent>(event.entity).y, event.y);
    }
}

void PlacementIsDeterministic()
{
    // Two identical worlds must produce identical maps, or replays and
    // reproducible bug reports are off the table.
    std::vector<int> firstRun;

    for (int run = 0; run < 3; ++run)
    {
        Simulation simulation(64, 64, true);
        simulation.Install<CoreSimulationModule>();
        simulation.Install<BroadcastModule>();
        simulation.Install<SpawnRulesModule>(DressMonster);
        AddSpawner(simulation, 30, 30, 3, 8);
        simulation.Start();

        std::vector<int> tiles;
        for (const Entity monster : LiveMonsters(simulation.World()))
        {
            const GridPositionComponent& position = simulation.World().registry.Get<GridPositionComponent>(monster);
            tiles.push_back(position.y * 64 + position.x);
        }

        if (run == 0)
        {
            firstRun = tiles;
            CHECK_EQ(firstRun.size(), 8u);
        }
        else
        {
            CHECK(tiles == firstRun);
        }
    }
}

void SeveralSpawnersAreIndependent()
{
    Simulation simulation(128, 128, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<SpawnRulesModule>(DressMonster);

    const Entity a = AddSpawner(simulation, 20, 20, 3, 4);
    const Entity b = AddSpawner(simulation, 80, 80, 3, 2);
    simulation.Start();

    CHECK_EQ(simulation.World().registry.Get<SpawnerComponent>(a).aliveCount, 0);

    simulation.Tick(kStep);
    CHECK_EQ(simulation.World().registry.Get<SpawnerComponent>(a).aliveCount, 4);
    CHECK_EQ(simulation.World().registry.Get<SpawnerComponent>(b).aliveCount, 2);

    // Wiping one must not make the other refill or stall.
    for (const Entity monster : LiveMonsters(simulation.World()))
    {
        if (simulation.World().registry.Get<SpawnedByComponent>(monster).spawner == a)
        {
            simulation.World().Despawn(monster);
        }
    }

    for (int i = 0; i < 30; ++i)
    {
        simulation.Tick(kStep);
    }

    CHECK_EQ(simulation.World().registry.Get<SpawnerComponent>(a).aliveCount, 4);
    CHECK_EQ(simulation.World().registry.Get<SpawnerComponent>(b).aliveCount, 2);
}

} // namespace

int main()
{
    PrimeFillsToStrength();
    ABurstSpreadsAcrossItsArea();
    MonstersMayShareATile();
    ACrowdedAreaStillReachesStrength();
    NoRoomAtAllIsHarmless();
    ALossIsReplacedAfterTheDelay();
    LossesComeBackOneAtATime();
    AFullSpawnerNeverOverfills();
    CountIsDerivedNotBookkept();
    MonstersOfADestroyedSpawnerCountForNobody();
    CombatDeathsAreReplaced();
    SpawnersAreNotOnTheMap();
    SpawnedEventDescribesAFinishedMonster();
    PlacementIsDeterministic();
    SeveralSpawnersAreIndependent();

    return world_v2::test::Summary("SpawnSystem");
}
