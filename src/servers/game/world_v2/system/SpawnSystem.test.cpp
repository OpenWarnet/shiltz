#include "../Simulation.h"
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
void DressMonster(MapWorld& world, Entity monster, std::uint32_t templateId)
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

std::vector<Entity> LiveMonsters(MapWorld& world)
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
    InstallSpawnRules(simulation.World(), DressMonster);

    const Entity spawner = AddSpawner(simulation, 20, 20, 3, 6);
    simulation.PrimeSpawns();

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

        // On the map and blocking, like any other creature.
        CHECK_EQ(simulation.World().tiles.OccupantAt(position.x, position.y), monster);
    }
}

void EveryMonsterGetsItsOwnTile()
{
    Simulation simulation(64, 64, true);
    InstallSpawnRules(simulation.World(), DressMonster);

    // Twenty monsters into a 5x5 area -- twenty of the twenty-five tiles.
    // Packed tightly enough that tile choice has to see the claims made by
    // the requests ahead of it in the same flush; choosing from walkability
    // alone leaves most of the burst with nowhere to land.
    //
    // The count is the real assertion here. SpawnBlocking refuses to stack
    // two creatures on one tile whatever the caller asks for, so the
    // distinctness check below can never fail on its own -- a broken
    // chooser shows up as monsters that never got placed.
    AddSpawner(simulation, 30, 30, 2, 20);
    simulation.PrimeSpawns();

    const std::vector<Entity> monsters = LiveMonsters(simulation.World());
    CHECK_EQ(monsters.size(), 20u);

    std::vector<int> tiles;
    for (const Entity monster : monsters)
    {
        const GridPositionComponent& position = simulation.World().registry.Get<GridPositionComponent>(monster);
        tiles.push_back(position.y * 64 + position.x);
    }

    std::sort(tiles.begin(), tiles.end());
    CHECK(std::adjacent_find(tiles.begin(), tiles.end()) == tiles.end());
}

void ACrowdedAreaFillsWhatItCan()
{
    Simulation simulation(64, 64, true);
    InstallSpawnRules(simulation.World(), DressMonster);

    // A 3x3 area asked for 20 monsters -- nine tiles, one of them walled.
    AddSpawner(simulation, 30, 30, 1, 20);
    simulation.World().tiles.SetWalkable(30, 30, false);
    simulation.PrimeSpawns();

    CHECK_EQ(LiveMonsters(simulation.World()).size(), 8u);
    CHECK_EQ(simulation.World().tiles.OccupantAt(30, 30), kNullEntity);

    // And it keeps asking without ever succeeding, rather than wedging.
    for (int i = 0; i < 20; ++i)
    {
        simulation.Tick(kStep);
    }
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 8u);
}

void NoRoomAtAllIsHarmless()
{
    Simulation simulation(64, 64, true);
    InstallSpawnRules(simulation.World(), DressMonster);

    AddSpawner(simulation, 30, 30, 0, 4);
    simulation.World().tiles.SetWalkable(30, 30, false);
    simulation.PrimeSpawns();

    CHECK_EQ(LiveMonsters(simulation.World()).size(), 0u);

    simulation.Tick(kStep);
    CHECK_EQ(LiveMonsters(simulation.World()).size(), 0u);
}

void ALossIsReplacedAfterTheDelay()
{
    Simulation simulation(64, 64, true);
    InstallSpawnRules(simulation.World(), DressMonster);

    AddSpawner(simulation, 20, 20, 3, 4);
    simulation.PrimeSpawns();
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
    InstallSpawnRules(simulation.World(), DressMonster);

    AddSpawner(simulation, 20, 20, 4, 6);
    simulation.PrimeSpawns();

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
    InstallSpawnRules(simulation.World(), DressMonster);

    const Entity spawner = AddSpawner(simulation, 20, 20, 4, 3);
    simulation.PrimeSpawns();

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
    InstallSpawnRules(simulation.World(), DressMonster);

    const Entity spawner = AddSpawner(simulation, 20, 20, 4, 5);
    simulation.PrimeSpawns();

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
    InstallSpawnRules(simulation.World(), DressMonster);

    const Entity first = AddSpawner(simulation, 20, 20, 3, 3);
    simulation.PrimeSpawns();
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
    InstallSpawnRules(simulation.World(), DressMonster);
    InstallCombatRules(simulation.World());

    AddSpawner(simulation, 20, 20, 3, 2);
    simulation.PrimeSpawns();

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
    InstallSpawnRules(simulation.World(), DressMonster);

    const Entity spawner = AddSpawner(simulation, 20, 20, 3, 2);
    simulation.PrimeSpawns();

    // A spawn point is bookkeeping, not a creature: nothing can walk into
    // it, target it, or see it.
    CHECK(!simulation.World().registry.Has<GridPositionComponent>(spawner));
    CHECK(!simulation.World().registry.Has<FactionComponent>(spawner));
    CHECK(simulation.World().tiles.OccupantAt(20, 20) != spawner);
    CHECK(simulation.World().tiles.IsFree(20, 20) || simulation.World().tiles.OccupantAt(20, 20) != kNullEntity);
}

void SpawnedEventDescribesAFinishedMonster()
{
    Simulation simulation(64, 64, true);

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

    InstallSpawnRules(simulation.World(), DressMonster);
    AddSpawner(simulation, 20, 20, 3, 3);
    simulation.PrimeSpawns();

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
        InstallSpawnRules(simulation.World(), DressMonster);
        AddSpawner(simulation, 30, 30, 3, 8);
        simulation.PrimeSpawns();

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
    InstallSpawnRules(simulation.World(), DressMonster);

    const Entity a = AddSpawner(simulation, 20, 20, 3, 4);
    const Entity b = AddSpawner(simulation, 80, 80, 3, 2);
    simulation.PrimeSpawns();

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
    EveryMonsterGetsItsOwnTile();
    ACrowdedAreaFillsWhatItCan();
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
