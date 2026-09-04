#include "../Simulation.h"
#include "../component/Combat.h"
#include "../component/Grid.h"
#include "../component/Items.h"
#include "../component/Request.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "../event/CombatEvents.h"
#include "CombatRules.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace world_v2;

namespace
{

constexpr int kMonsters = 1;
constexpr int kPlayers = 2;

Entity SpawnKiller(Simulation& simulation, int x, int y, std::uint64_t requiredForNextLevel)
{
    MapWorld& world = simulation.World();
    const Entity entity = world.Spawn(x, y);
    world.registry.Assign<FactionComponent>(entity, kPlayers);
    world.registry.Assign<HealthComponent>(entity, 100, 100);
    world.registry.Assign<ExperienceComponent>(entity, std::uint64_t{0}, std::uint32_t{1}, requiredForNextLevel);
    return entity;
}

Entity SpawnVictim(Simulation& simulation, int x, int y, int health, std::uint64_t reward, std::uint32_t dropTable)
{
    MapWorld& world = simulation.World();
    const Entity entity = world.Spawn(x, y);
    world.registry.Assign<FactionComponent>(entity, kMonsters);
    world.registry.Assign<HealthComponent>(entity, health, health);
    world.registry.Assign<ExperienceRewardComponent>(entity, reward);
    if (dropTable != 0)
    {
        world.registry.Assign<LootTableComponent>(entity, dropTable);
    }
    return entity;
}

void OneKillResolvesCompletelyInOneTick()
{
    Simulation simulation(32, 32, true);
    InstallCombatRules(simulation.World());

    const Entity killer = SpawnKiller(simulation, 5, 5, 100);
    const Entity victim = SpawnVictim(simulation, 6, 5, 5, 100, 42);

    std::vector<LootDropEvent> drops;
    bool corpseGoneAtDropTime = false;
    simulation.World().events.Listen<LootDropEvent>(
        [&](const LootDropEvent& event)
        {
            drops.push_back(event);
            // The despawn happened in the death handler, a flush round
            // earlier -- which is exactly why this event carries
            // coordinates instead of a handle to look them up from.
            corpseGoneAtDropTime = !simulation.World().registry.Exists(event.source);
        });

    std::vector<LevelUpEvent> levels;
    simulation.World().events.Listen<LevelUpEvent>([&levels](const LevelUpEvent& event) { levels.push_back(event); });

    simulation.World().registry.Assign<AttackRequestComponent>(killer, victim, 5);
    simulation.Tick(0.0f);

    // Damage, death, credit, drop and despawn all inside the one tick that
    // caused them -- not one link per tick.
    CHECK(!simulation.World().registry.Exists(victim));
    CHECK(simulation.World().tiles.OccupantsAt(6, 5).empty());

    const ExperienceComponent& experience = simulation.World().registry.Get<ExperienceComponent>(killer);
    CHECK_EQ(experience.level, 2u);
    CHECK_EQ(experience.current, 0u);

    CHECK_EQ(drops.size(), 1u);
    if (drops.size() == 1)
    {
        CHECK_EQ(drops[0].dropTableId, 42u);
        CHECK_EQ(drops[0].x, 6);
        CHECK_EQ(drops[0].y, 5);
    }
    CHECK(corpseGoneAtDropTime);

    CHECK_EQ(levels.size(), 1u);
    if (levels.size() == 1)
    {
        CHECK_EQ(levels[0].entity, killer);
        CHECK_EQ(levels[0].level, 2u);
    }

    // Nothing left over for the next tick.
    CHECK_EQ(simulation.World().events.PendingCount(), 0u);
}

void CascadeOrderIsDeathThenCreditThenLevel()
{
    Simulation simulation(32, 32, true);
    InstallCombatRules(simulation.World());

    const Entity killer = SpawnKiller(simulation, 5, 5, 100);
    const Entity victim = SpawnVictim(simulation, 6, 5, 5, 100, 42);

    std::vector<std::string> log;
    simulation.World().events.Listen<DamageDealtEvent>([&log](const DamageDealtEvent&) { log.push_back("damage"); });
    simulation.World().events.Listen<DeathEvent>([&log](const DeathEvent&) { log.push_back("death"); });
    simulation.World().events.Listen<ExperienceAwardEvent>(
        [&log](const ExperienceAwardEvent&) { log.push_back("experience"); });
    simulation.World().events.Listen<LootDropEvent>([&log](const LootDropEvent&) { log.push_back("loot"); });
    simulation.World().events.Listen<LevelUpEvent>([&log](const LevelUpEvent&) { log.push_back("level"); });

    simulation.World().registry.Assign<AttackRequestComponent>(killer, victim, 5);
    simulation.Tick(0.0f);

    const std::vector<std::string> expected{"damage", "death", "experience", "loot", "level"};
    CHECK(log == expected);
}

void ExperienceBelowTheThresholdDoesNotLevel()
{
    Simulation simulation(32, 32, true);
    InstallCombatRules(simulation.World());

    const Entity killer = SpawnKiller(simulation, 5, 5, 100);
    const Entity victim = SpawnVictim(simulation, 6, 5, 5, 30, 0);

    int levelUps = 0;
    simulation.World().events.Listen<LevelUpEvent>([&levelUps](const LevelUpEvent&) { ++levelUps; });

    simulation.World().registry.Assign<AttackRequestComponent>(killer, victim, 5);
    simulation.Tick(0.0f);

    const ExperienceComponent& experience = simulation.World().registry.Get<ExperienceComponent>(killer);
    CHECK_EQ(experience.level, 1u);
    CHECK_EQ(experience.current, 30u);
    CHECK_EQ(levelUps, 0);
}

void OneAwardCanGrantSeveralLevels()
{
    Simulation simulation(32, 32, true);
    InstallCombatRules(simulation.World());

    const Entity killer = SpawnKiller(simulation, 5, 5, 100);
    const Entity victim = SpawnVictim(simulation, 6, 5, 5, 350, 0);

    std::vector<std::uint32_t> levels;
    simulation.World().events.Listen<LevelUpEvent>(
        [&levels](const LevelUpEvent& event) { levels.push_back(event.level); });

    simulation.World().registry.Assign<AttackRequestComponent>(killer, victim, 5);
    simulation.Tick(0.0f);

    const ExperienceComponent& experience = simulation.World().registry.Get<ExperienceComponent>(killer);
    CHECK_EQ(experience.level, 4u);
    CHECK_EQ(experience.current, 50u);

    const std::vector<std::uint32_t> expected{2, 3, 4};
    CHECK(levels == expected);
}

void AZeroThresholdCannotSpin()
{
    Simulation simulation(32, 32, true);
    InstallCombatRules(simulation.World());

    // A game that never sets requiredForNextLevel would otherwise loop
    // forever subtracting nothing.
    const Entity killer = SpawnKiller(simulation, 5, 5, 0);
    const Entity victim = SpawnVictim(simulation, 6, 5, 5, 500, 0);

    int levelUps = 0;
    simulation.World().events.Listen<LevelUpEvent>([&levelUps](const LevelUpEvent&) { ++levelUps; });

    simulation.World().registry.Assign<AttackRequestComponent>(killer, victim, 5);
    simulation.Tick(0.0f);

    CHECK_EQ(simulation.World().registry.Get<ExperienceComponent>(killer).current, 500u);
    CHECK_EQ(levelUps, 0);
}

void NoDropTableMeansNoDropEvent()
{
    Simulation simulation(32, 32, true);
    InstallCombatRules(simulation.World());

    const Entity killer = SpawnKiller(simulation, 5, 5, 100);
    const Entity victim = SpawnVictim(simulation, 6, 5, 5, 10, 0);

    int drops = 0;
    simulation.World().events.Listen<LootDropEvent>([&drops](const LootDropEvent&) { ++drops; });

    simulation.World().registry.Assign<AttackRequestComponent>(killer, victim, 5);
    simulation.Tick(0.0f);

    CHECK_EQ(drops, 0);
    CHECK(!simulation.World().registry.Exists(victim));
}

void AMutualKillPaysNeitherSide()
{
    Simulation simulation(32, 32, true);
    InstallCombatRules(simulation.World());

    // Both on one hit point, both swinging. Combat resolves both requests
    // before death announces either, so both fall in the same tick.
    const Entity first = SpawnKiller(simulation, 5, 5, 100);
    simulation.World().registry.Assign<ExperienceRewardComponent>(first, std::uint64_t{100});
    simulation.World().registry.Get<HealthComponent>(first).current = 1;

    const Entity second = SpawnKiller(simulation, 6, 5, 100);
    simulation.World().registry.Assign<ExperienceRewardComponent>(second, std::uint64_t{100});
    simulation.World().registry.Get<HealthComponent>(second).current = 1;

    int awards = 0;
    simulation.World().events.Listen<ExperienceAwardEvent>([&awards](const ExperienceAwardEvent&) { ++awards; });

    simulation.World().registry.Assign<AttackRequestComponent>(first, second, 5);
    simulation.World().registry.Assign<AttackRequestComponent>(second, first, 5);
    simulation.Tick(0.0f);

    // Whichever death is dispatched first, the other party is already
    // tagged dead -- so neither gets paid, rather than whoever happened to
    // be handled second.
    CHECK_EQ(awards, 0);
    CHECK(!simulation.World().registry.Exists(first));
    CHECK(!simulation.World().registry.Exists(second));
    CHECK(simulation.World().tiles.OccupantsAt(5, 5).empty());
    CHECK(simulation.World().tiles.OccupantsAt(6, 5).empty());
}

void LootHandlerCanSpawnAtTheBarrier()
{
    Simulation simulation(32, 32, true);
    InstallCombatRules(simulation.World());

    const Entity killer = SpawnKiller(simulation, 5, 5, 100);
    const Entity victim = SpawnVictim(simulation, 6, 5, 5, 10, 7);

    // What the game layer would really do with a drop: turn it into a
    // ground item. Spawning is structural, and the barrier is the one place
    // it is legal -- this proves that path works, not just despawning.
    Entity dropped = kNullEntity;
    simulation.World().events.Listen<LootDropEvent>(
        [&](const LootDropEvent& event) { dropped = simulation.World().Spawn(event.x, event.y); });

    simulation.World().registry.Assign<AttackRequestComponent>(killer, victim, 5);
    simulation.Tick(0.0f);

    CHECK(dropped != kNullEntity);
    CHECK(simulation.World().registry.Exists(dropped));
    CHECK_EQ(simulation.World().registry.Get<GridPositionComponent>(dropped).x, 6);

    // The corpse came off the tile and the drop took its place on it. Both
    // are listed by the same index now -- what changed is that the item
    // being there stops nobody, so the square is as enterable as it was.
    CHECK(!simulation.World().tiles.Contains(victim, 6, 5));
    CHECK(simulation.World().tiles.Contains(dropped, 6, 5));
    CHECK(simulation.World().tiles.IsWalkable(6, 5));
}

void AiDrivenKillOverSeveralTicks()
{
    Simulation simulation(32, 32, true);
    InstallCombatRules(simulation.World());

    // A monster that has to close distance before it can hit anything.
    const Entity monster = simulation.World().Spawn(5, 5);
    simulation.World().registry.Assign<FactionComponent>(monster, kMonsters);
    simulation.World().registry.Assign<AIComponent>(monster, 10, 1, kNullEntity);
    simulation.World().registry.Assign<HealthComponent>(monster, 100, 100);
    simulation.World().registry.Assign<AttackPowerComponent>(monster, 4);

    const Entity player = simulation.World().Spawn(11, 5);
    simulation.World().registry.Assign<FactionComponent>(player, kPlayers);
    simulation.World().registry.Assign<HealthComponent>(player, 12, 12);

    int deaths = 0;
    simulation.World().events.Listen<DeathEvent>([&deaths](const DeathEvent&) { ++deaths; });

    // Five tiles to close, then three hits at 4 damage against 12 health.
    for (int tick = 0; tick < 20 && deaths == 0; ++tick)
    {
        simulation.Tick(0.25f);
    }

    CHECK_EQ(deaths, 1);
    CHECK(!simulation.World().registry.Exists(player));

    // The monster is still holding the handle it killed: AI ran before
    // combat in that tick, and the despawn happened at the barrier after
    // both. The handle is stale, not dangling -- which is the whole point
    // of the generation bits.
    CHECK_EQ(simulation.World().registry.Get<AIComponent>(monster).currentTarget, player);
    CHECK(!simulation.World().registry.Exists(player));

    // The next AI pass is where it notices and lets go, and with nothing
    // left to fight it issues no further requests.
    simulation.Tick(0.25f);
    CHECK_EQ(simulation.World().registry.Get<AIComponent>(monster).currentTarget, kNullEntity);
    CHECK(!simulation.World().registry.Has<AttackRequestComponent>(monster));
    CHECK_EQ(deaths, 1);
}

} // namespace

int main()
{
    OneKillResolvesCompletelyInOneTick();
    CascadeOrderIsDeathThenCreditThenLevel();
    ExperienceBelowTheThresholdDoesNotLevel();
    OneAwardCanGrantSeveralLevels();
    AZeroThresholdCannotSpin();
    NoDropTableMeansNoDropEvent();
    AMutualKillPaysNeitherSide();
    LootHandlerCanSpawnAtTheBarrier();
    AiDrivenKillOverSeveralTicks();

    return world_v2::test::Summary("CombatRules");
}
