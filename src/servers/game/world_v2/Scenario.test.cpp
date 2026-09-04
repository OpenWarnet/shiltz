// End-to-end exercise of the whole framework.
//
// Every other test in world_v2 pins down one piece. This one runs all of
// them together, for hundreds of ticks, on a map with walls: commands
// arriving from outside, spawners filling and refilling, monsters hunting
// through corridors, combat, deaths cascading into loot and experience, and
// notices going back out to viewers -- while checking after every single
// tick that the world is still internally consistent.
//
// The invariants are the point. Most of the ways this framework can go
// wrong are not a wrong answer to one question but a slow divergence
// between two things that are supposed to describe the same fact -- an
// entity's tile and the occupancy grid's idea of it, a spawner's count and
// the monsters that actually exist. Those drift silently and only surface
// much later as creatures standing inside each other or camps that never
// refill. Checking them every tick is what turns that into a test failure
// with a tick number on it.

#include "Simulation.h"
#include "component/Combat.h"
#include "component/Grid.h"
#include "component/Items.h"
#include "component/Network.h"
#include "component/Request.h"
#include "component/Spawn.h"
#include "world/MapWorld.h"
#include "core/Entity.h"
#include "core/Test.h"
#include "event/CombatEvents.h"
#include "event/ItemEvents.h"
#include "event/SpawnEvents.h"
#include "system/BroadcastSystem.h"
#include "system/CombatRules.h"
#include "system/ItemRules.h"
#include "system/SpawnRules.h"
#include "world/Inventory.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

using namespace world_v2;

namespace
{

constexpr int kMapSize = 96;
constexpr int kPlayerFaction = 1;
constexpr int kMonsterFaction = 2;
constexpr int kPlayerCount = 8;
constexpr int kSpawnerCount = 6;
constexpr int kMonstersPerSpawner = 5;
constexpr float kTickSeconds = 0.25f;

// Marks everything that claims a tile, so the consistency check can verify
// the occupancy grid in both directions -- no stale occupants, and no
// blocking entity that has quietly lost its tile.
struct BlockingTag
{
};

// Deterministic mixing, used both to drive the scenario and to digest it.
// Nothing in the simulation is random; the whole run has to be reproducible
// bit for bit or the determinism check below means nothing.
std::uint32_t Hash(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t value = a * 0x9E3779B9u ^ (b + 0x165667B1u + (a << 6) + (a >> 2));
    value ^= value >> 15;
    value *= 0x2545F491u;
    value ^= value >> 13;
    return value;
}

struct MoveCommand
{
    Entity entity = kNullEntity;
    int directionX = 0;
    int directionY = 0;
};

struct AttackCommand
{
    Entity attacker = kNullEntity;
    Entity target = kNullEntity;
    int damage = 0;
};

struct PickupCommand
{
    Entity picker = kNullEntity;
    Entity item = kNullEntity;
};

class Scenario
{
public:
    Scenario()
        : m_simulation(kMapSize, kMapSize, true)
    {
        CarveTerrain();
        WireCommands();

        // Before WireRules on purpose. The loss bookkeeping below reads a
        // dying player's inventory, and CombatRules' death handler is the
        // one that despawns them -- registering first is what puts this
        // ahead of it in the listener order. Test bookkeeping, not
        // framework behaviour: nothing in world_v2 depends on listener
        // order, because every event carries what its handlers need.
        WireNotices();

        WireRules();
        PlaceSpawners();
        PlacePlayers();

        m_simulation.PrimeSpawns();
    }

    // One tick: issue everyone's orders through the queue, advance, then
    // audit.
    void Step(int tick)
    {
        IssuePlayerOrders(tick);
        m_simulation.Tick(kTickSeconds);
        CheckInvariants(tick);
        Digest();
    }

    std::uint64_t DigestValue() const
    {
        return m_digest;
    }

    std::size_t Deaths() const
    {
        return m_deaths;
    }

    std::size_t Spawns() const
    {
        return m_spawns;
    }

    std::size_t Notices() const
    {
        return m_notices;
    }

    std::size_t LootDrops() const
    {
        return m_lootDrops;
    }

    std::size_t Pickups() const
    {
        return m_pickups;
    }

    std::size_t LevelUps() const
    {
        return m_levelUps;
    }

    int Failures() const
    {
        return m_failures;
    }

    Simulation& Sim()
    {
        return m_simulation;
    }

    const std::vector<Entity>& Players() const
    {
        return m_players;
    }

private:
    void CarveTerrain()
    {
        // Vertical walls every eight columns, with a gap every sixteen
        // rows. Enough structure that monsters have to path around
        // something and that spawners can be walled into corners -- and
        // enough that "no entity ever stands on an unwalkable tile" is a
        // claim with teeth.
        for (int x = 8; x < kMapSize; x += 8)
        {
            for (int y = 0; y < kMapSize; ++y)
            {
                if (y % 16 != 0)
                {
                    m_simulation.World().tiles.SetWalkable(x, y, false);
                }
            }
        }
    }

    void WireCommands()
    {
        m_simulation.Commands().On<PickupCommand>(
            [](MapWorld& world, const PickupCommand& command)
            {
                if (!world.registry.Exists(command.picker) || !world.registry.Exists(command.item))
                {
                    return;
                }
                world.registry.Assign<PickupItemRequestComponent>(command.picker, command.item);
            });

        m_simulation.Commands().On<MoveCommand>(
            [](MapWorld& world, const MoveCommand& command)
            {
                // A player can have died between the push and the drain.
                if (!world.registry.Exists(command.entity))
                {
                    return;
                }
                world.registry.Assign<MoveIntentComponent>(command.entity, command.directionX, command.directionY);
            });

        m_simulation.Commands().On<AttackCommand>(
            [](MapWorld& world, const AttackCommand& command)
            {
                if (!world.registry.Exists(command.attacker) || !world.registry.Exists(command.target))
                {
                    return;
                }
                world.registry.Assign<AttackRequestComponent>(command.attacker, command.target, command.damage);
            });
    }

    void WireRules()
    {
        InstallCombatRules(m_simulation.World());
        InstallItemRules(m_simulation.World());

        // LootDropEvent is the seam world_v2 deliberately leaves unhandled,
        // because turning a table id into items needs .scr data it does not
        // read. This is a game layer standing in for that: one item per
        // corpse, the table id doubling as the item id.
        m_simulation.World().events.Listen<LootDropEvent>(
            [this](const LootDropEvent& event)
            {
                SpawnGroundItem(m_simulation.World(), event.dropTableId, 3, 10, event.x, event.y);
            });

        InstallSpawnRules(m_simulation.World(),
                          [](MapWorld& world, Entity monster, std::uint32_t templateId)
                          {
                              world.registry.Assign<BlockingTag>(monster);
                              world.registry.Assign<FactionComponent>(monster, kMonsterFaction);
                              world.registry.Assign<HealthComponent>(monster, 24, 24);
                              world.registry.Assign<AIComponent>(monster, 7, 1, kNullEntity);
                              world.registry.Assign<AttackPowerComponent>(monster, 2);
                              world.registry.Assign<ExperienceRewardComponent>(monster, std::uint64_t{40});
                              world.registry.Assign<LootTableComponent>(monster, templateId);
                          });
    }

    void WireNotices()
    {
        m_simulation.OnNotice(
            [this](Entity, const Notice& notice)
            {
                ++m_notices;
                // Fold the outbound stream into the digest too: two
                // identical runs must tell their viewers exactly the same
                // story, not merely end in the same state.
                Mix(static_cast<std::uint64_t>(notice.kind));
                Mix(notice.subject);
                Mix(static_cast<std::uint64_t>(notice.x));
                Mix(static_cast<std::uint64_t>(notice.y));
            });

        m_simulation.World().events.Listen<GroundItemSpawnedEvent>(
            [this](const GroundItemSpawnedEvent& event) { m_itemsSpawned += event.quantity; });

        m_simulation.World().events.Listen<ItemPickedUpEvent>([this](const ItemPickedUpEvent&) { ++m_pickups; });

        m_simulation.World().events.Listen<DeathEvent>(
            [this](const DeathEvent& event)
            {
                ++m_deaths;

                // Anything a dying entity was carrying leaves the world with
                // it. Counted here so the conservation check below stays an
                // exact equality rather than an inequality that would hide a
                // duplication bug.
                if (const InventoryComponent* inventory =
                        m_simulation.World().registry.TryGet<InventoryComponent>(event.entity))
                {
                    for (const InventoryComponent::Slot& slot : inventory->slots)
                    {
                        m_itemsLost += slot.quantity;
                    }
                }
            });
        m_simulation.World().events.Listen<MonsterSpawnedEvent>([this](const MonsterSpawnedEvent&) { ++m_spawns; });
        m_simulation.World().events.Listen<LootDropEvent>([this](const LootDropEvent&) { ++m_lootDrops; });
        m_simulation.World().events.Listen<LevelUpEvent>([this](const LevelUpEvent&) { ++m_levelUps; });
    }

    void PlaceSpawners()
    {
        for (int i = 0; i < kSpawnerCount; ++i)
        {
            const int x = 3 + (i % 3) * 32;
            const int y = 8 + (i / 3) * 40;

            const Entity spawner = m_simulation.World().registry.Create();
            m_simulation.World().registry.Assign<SpawnerComponent>(
                spawner, static_cast<std::uint32_t>(500 + i), x, y, 3, kMonstersPerSpawner, 0, 2.0f, 0.0f,
                std::uint32_t{0});
            m_spawners.push_back(spawner);
        }
    }

    // Walks outward from (x, y) for somewhere a player can actually stand.
    // The terrain has walls in it, so a fixed grid of start positions puts
    // some of them inside one.
    Entity SpawnNear(int x, int y)
    {
        for (int radius = 0; radius < kMapSize; ++radius)
        {
            for (int dy = -radius; dy <= radius; ++dy)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    const Entity placed = m_simulation.World().SpawnBlocking(x + dx, y + dy);
                    if (placed != kNullEntity)
                    {
                        return placed;
                    }
                }
            }
        }

        return kNullEntity;
    }

    void PlacePlayers()
    {
        for (int i = 0; i < kPlayerCount; ++i)
        {
            const int x = 2 + (i % 4) * 2;
            const int y = 40 + (i / 4) * 2;

            const Entity player = SpawnNear(x, y);
            CHECK(player != kNullEntity);
            if (player == kNullEntity)
            {
                continue;
            }

            m_simulation.World().registry.Assign<BlockingTag>(player);
            m_simulation.World().registry.Assign<FactionComponent>(player, kPlayerFaction);

            // Tough enough to survive the whole run, so the scenario keeps
            // producing traffic instead of going quiet halfway through.
            // Player death gets its own focused test below.
            m_simulation.World().registry.Assign<HealthComponent>(player, 100000, 100000);
            m_simulation.World().registry.Assign<ExperienceComponent>(player, std::uint64_t{0}, std::uint32_t{1},
                                                                      std::uint64_t{120});
            m_simulation.World().registry.Assign<ViewerComponent>(player, 12);
            m_simulation.World().registry.Assign<InventoryComponent>(player, MakeInventory(12));
            m_players.push_back(player);
        }
    }

    // Everything a player does goes through CommandQueue, exactly as a
    // packet handler on a connection thread would -- the simulation is
    // never poked directly.
    void IssuePlayerOrders(int tick)
    {
        for (std::size_t i = 0; i < m_players.size(); ++i)
        {
            const Entity player = m_players[i];
            if (!m_simulation.World().registry.Exists(player))
            {
                continue;
            }

            const std::uint32_t roll = Hash(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(tick));

            // Grab loot underfoot first, then swing at whatever is
            // adjacent, then wander.
            const Entity loot = NearbyItem(player);
            if (loot != kNullEntity)
            {
                m_simulation.Commands().Push(PickupCommand{player, loot});
                continue;
            }

            const Entity target = AdjacentEnemy(player);
            if (target != kNullEntity && (roll % 3) != 0)
            {
                m_simulation.Commands().Push(AttackCommand{player, target, 6});
                continue;
            }

            const int direction = static_cast<int>(roll % 9);
            m_simulation.Commands().Push(MoveCommand{player, (direction % 3) - 1, (direction / 3) - 1});
        }
    }

    // Ground items are not in the occupancy grid -- that is the point of
    // them -- so finding one means asking the registry rather than the map.
    Entity NearbyItem(Entity player)
    {
        const GridPositionComponent* position = m_simulation.World().registry.TryGet<GridPositionComponent>(player);
        if (position == nullptr)
        {
            return kNullEntity;
        }

        const int px = position->x;
        const int py = position->y;

        Entity found = kNullEntity;
        m_simulation.World().registry.view<GroundItemComponent, GridPositionComponent>().Each(
            [&](Entity item, GroundItemComponent& ground, GridPositionComponent& itemPosition)
            {
                if (found != kNullEntity || ground.quantity == 0)
                {
                    return;
                }

                const int dx = itemPosition.x - px;
                const int dy = itemPosition.y - py;
                if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1)
                {
                    found = item;
                }
            });

        return found;
    }

    Entity AdjacentEnemy(Entity player)
    {
        const GridPositionComponent* position = m_simulation.World().registry.TryGet<GridPositionComponent>(player);
        if (position == nullptr)
        {
            return kNullEntity;
        }

        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx == 0 && dy == 0)
                {
                    continue;
                }

                const Entity occupant = m_simulation.World().tiles.OccupantAt(position->x + dx, position->y + dy);
                if (occupant == kNullEntity)
                {
                    continue;
                }

                const FactionComponent* faction = m_simulation.World().registry.TryGet<FactionComponent>(occupant);
                if (faction != nullptr && faction->faction == kMonsterFaction)
                {
                    return occupant;
                }
            }
        }

        return kNullEntity;
    }

    // The audit. Runs after every tick; a failure names the tick it broke
    // on.
    void CheckInvariants(int tick)
    {
        MapWorld& world = m_simulation.World();

        // 1. Forward: every tile the grid says is occupied holds a living
        //    entity that agrees it is standing there. A stale occupant is a
        //    phantom wall nothing can ever walk through again.
        std::size_t occupiedTiles = 0;
        for (int y = 0; y < kMapSize; ++y)
        {
            for (int x = 0; x < kMapSize; ++x)
            {
                const Entity occupant = world.tiles.OccupantAt(x, y);
                if (occupant == kNullEntity)
                {
                    continue;
                }

                ++occupiedTiles;

                if (!world.registry.Exists(occupant))
                {
                    Fail(tick, "grid holds a destroyed entity");
                    continue;
                }

                const GridPositionComponent* position = world.registry.TryGet<GridPositionComponent>(occupant);
                if (position == nullptr || position->x != x || position->y != y)
                {
                    Fail(tick, "grid and GridPositionComponent disagree");
                }

                if (!world.tiles.IsWalkable(x, y))
                {
                    Fail(tick, "entity standing on unwalkable terrain");
                }
            }
        }

        // 2. Reverse: every entity that claims a tile is the one the grid
        //    has there. Catches the opposite drift -- a creature that lost
        //    its claim and can now be walked through.
        std::size_t blockingEntities = 0;
        world.registry.view<BlockingTag, GridPositionComponent>().Each(
            [&](Entity entity, BlockingTag&, GridPositionComponent& position)
            {
                ++blockingEntities;
                if (world.tiles.OccupantAt(position.x, position.y) != entity)
                {
                    Fail(tick, "blocking entity is not the occupant of its own tile");
                }
            });

        if (occupiedTiles != blockingEntities)
        {
            Fail(tick, "occupied tile count does not match blocking entity count");
        }

        // 3. Nothing structural is left half-done. Deaths resolve at the
        //    barrier, so no corpse survives a completed tick, and every
        //    attack request is consumed by the system that reads it.
        if (world.registry.Count<DeadComponent>() != 0)
        {
            Fail(tick, "a corpse survived the barrier");
        }
        if (world.registry.Count<AttackRequestComponent>() != 0)
        {
            Fail(tick, "an attack request went unconsumed");
        }

        // 4. No negative health reached anything downstream.
        world.registry.view<HealthComponent>().Each(
            [&](Entity, HealthComponent& health)
            {
                if (health.current < 0)
                {
                    Fail(tick, "negative health");
                }
            });

        // 5. Spawners never overfill.
        //
        // Note what is deliberately NOT asserted: that aliveCount equals
        // the monsters that currently exist. It does not, and should not.
        // The count is recomputed at the top of stage 2, and the tick then
        // goes on to kill things (removed at the barrier) and spawn things
        // (created at the barrier), so by the time this audit runs it is a
        // snapshot from earlier in the same tick. It converges on the next
        // recount, which is the whole design -- a derived count that
        // repairs itself rather than one maintained by hand.
        //
        // The claim that does hold at every instant is the ceiling: no
        // spawner has more than it asked for, and the population never
        // exceeds the sum of what every spawner asked for.
        std::size_t linkedMonsters = 0;
        world.registry.view<SpawnedByComponent>().Each([&](Entity, SpawnedByComponent&) { ++linkedMonsters; });

        std::size_t totalDesired = 0;
        world.registry.view<SpawnerComponent>().Each(
            [&](Entity, SpawnerComponent& definition)
            {
                if (definition.aliveCount > definition.desiredCount)
                {
                    Fail(tick, "spawner reports more alive than it wants");
                }
                totalDesired += static_cast<std::size_t>(definition.desiredCount);
            });

        if (linkedMonsters > totalDesired)
        {
            Fail(tick, "more monsters exist than every spawner combined asked for");
        }

        // 6. Items are conserved. Everything ever dropped is either still
        //    on the ground, in somebody's bag, or went into the grave with
        //    whoever was carrying it -- exactly, with nothing over.
        //
        //    This is the sharpest tool here. A duplication bug shows up as
        //    a surplus and a loss bug as a shortfall, both immediately and
        //    both with a tick number, where either could otherwise run for
        //    hours before anyone noticed the economy was wrong.
        std::uint64_t onGround = 0;
        std::size_t groundItems = 0;
        world.registry.view<GroundItemComponent>().Each(
            [&](Entity, GroundItemComponent& ground)
            {
                ++groundItems;
                onGround += ground.quantity;

                // A claimed item is despawned at the barrier, so none may
                // still be lying about with nothing left on it.
                if (ground.quantity == 0)
                {
                    Fail(tick, "a claimed ground item outlived the barrier");
                }
            });

        std::uint64_t carried = 0;
        std::size_t players = 0;
        world.registry.view<ViewerComponent>().Each(
            [&](Entity player, ViewerComponent&)
            {
                ++players;
                if (const InventoryComponent* inventory = world.registry.TryGet<InventoryComponent>(player))
                {
                    for (const InventoryComponent::Slot& slot : inventory->slots)
                    {
                        carried += slot.quantity;
                    }
                }
            });

        if (onGround + carried + m_itemsLost != m_itemsSpawned)
        {
            Fail(tick, "items are not conserved");
        }

        // 7. Every entity belongs to exactly one category the scenario
        //    knows about. A stray -- something created and never reclaimed,
        //    or reclaimed twice -- shows up as a mismatch rather than as
        //    slow growth nobody is watching.
        std::size_t spawners = 0;
        world.registry.view<SpawnerComponent>().Each([&](Entity, SpawnerComponent&) { ++spawners; });

        if (players + spawners + linkedMonsters + groundItems != world.registry.AliveCount())
        {
            Fail(tick, "an entity belongs to no category the scenario accounts for");
        }
    }

    void Digest()
    {
        MapWorld& world = m_simulation.World();

        Mix(world.registry.AliveCount());
        Mix(m_deaths);
        Mix(m_spawns);
        Mix(m_itemsSpawned);
        Mix(m_pickups);

        // Walked in tile order rather than entity order, so the digest does
        // not depend on internal pool layout -- only on where things
        // actually are.
        for (int y = 0; y < kMapSize; ++y)
        {
            for (int x = 0; x < kMapSize; ++x)
            {
                const Entity occupant = world.tiles.OccupantAt(x, y);
                if (occupant == kNullEntity)
                {
                    continue;
                }

                Mix(static_cast<std::uint64_t>(y) * kMapSize + x);
                const HealthComponent* health = world.registry.TryGet<HealthComponent>(occupant);
                Mix(health != nullptr ? static_cast<std::uint64_t>(health->current) : 0u);
            }
        }
    }

    void Mix(std::uint64_t value)
    {
        m_digest ^= value;
        m_digest *= 1099511628211ull;
    }

    void Fail(int tick, const char* what)
    {
        if (m_failures < 8)
        {
            std::printf("  FAIL  tick %d: %s\n", tick, what);
        }
        ++m_failures;
        ++world_v2::test::g_failures;
    }

    Simulation m_simulation;
    std::vector<Entity> m_players;
    std::vector<Entity> m_spawners;

    std::uint64_t m_digest = 14695981039346656037ull;
    std::size_t m_notices = 0;
    std::size_t m_deaths = 0;
    std::size_t m_spawns = 0;
    std::size_t m_lootDrops = 0;
    std::size_t m_levelUps = 0;
    std::size_t m_pickups = 0;
    std::uint64_t m_itemsSpawned = 0;
    std::uint64_t m_itemsLost = 0;
    int m_failures = 0;
};

void TheWorldStaysConsistentUnderLoad()
{
    Scenario scenario;

    for (int tick = 0; tick < 300; ++tick)
    {
        scenario.Step(tick);
    }

    CHECK_EQ(scenario.Failures(), 0);

    // The run has to have actually exercised something. A scenario that
    // quietly did nothing would satisfy every invariant above.
    CHECK(scenario.Deaths() > 20);
    CHECK(scenario.Spawns() > static_cast<std::size_t>(kSpawnerCount * kMonstersPerSpawner));
    CHECK(scenario.Notices() > 1000);
    CHECK(scenario.LootDrops() > 20);
    CHECK(scenario.LevelUps() > 0);
    CHECK(scenario.Pickups() > 5);

    std::printf("  ..    %zu deaths, %zu spawns, %zu loot drops, %zu pickups, %zu level-ups, %zu notices\n",
                scenario.Deaths(), scenario.Spawns(), scenario.LootDrops(), scenario.Pickups(), scenario.LevelUps(),
                scenario.Notices());
}

void TwoIdenticalRunsAgreeExactly()
{
    // Nothing in the simulation is random, so the same inputs must produce
    // the same world and the same outbound stream -- not merely a similar
    // one. Without this, a bug report is not reproducible and a replay is
    // not possible.
    std::uint64_t first = 0;
    std::size_t firstDeaths = 0;
    std::size_t firstNotices = 0;

    for (int run = 0; run < 2; ++run)
    {
        Scenario scenario;
        for (int tick = 0; tick < 150; ++tick)
        {
            scenario.Step(tick);
        }

        CHECK_EQ(scenario.Failures(), 0);

        if (run == 0)
        {
            first = scenario.DigestValue();
            firstDeaths = scenario.Deaths();
            firstNotices = scenario.Notices();
        }
        else
        {
            CHECK_EQ(scenario.DigestValue(), first);
            CHECK_EQ(scenario.Deaths(), firstDeaths);
            CHECK_EQ(scenario.Notices(), firstNotices);
        }
    }
}

void APlayerDyingMidRunIsHandledCleanly()
{
    Scenario scenario;

    for (int tick = 0; tick < 40; ++tick)
    {
        scenario.Step(tick);
    }

    // Kill one outright, then keep running. Its queued commands, the
    // monsters targeting it, and the tile it was standing on all have to
    // resolve without anything drifting.
    const Entity victim = scenario.Players().front();
    CHECK(scenario.Sim().World().registry.Exists(victim));

    const GridPositionComponent& position = scenario.Sim().World().registry.Get<GridPositionComponent>(victim);
    const int deathX = position.x;
    const int deathY = position.y;

    scenario.Sim().World().registry.Get<HealthComponent>(victim).current = 0;

    for (int tick = 40; tick < 140; ++tick)
    {
        scenario.Step(tick);
    }

    CHECK_EQ(scenario.Failures(), 0);
    CHECK(!scenario.Sim().World().registry.Exists(victim));

    // Its tile went back to the map rather than becoming a phantom wall.
    // (Something else may well be standing there by now -- what matters is
    // that whatever is there is not the dead player.)
    CHECK(scenario.Sim().World().tiles.OccupantAt(deathX, deathY) != victim);
}

void CommandsFromManyThreadsSurviveALiveSimulation()
{
    // The seam under its real shape: connection threads pushing while the
    // simulation thread ticks. Determinism cannot hold here -- arrival
    // order is genuinely nondeterministic -- but nothing may be lost, and
    // every invariant still has to hold on every tick.
    Scenario scenario;

    // Bounded on purpose. An unthrottled push loop does not test the seam
    // any harder -- it just buries the simulation under millions of queued
    // commands, and the tick never finishes. A fixed budget per thread
    // keeps the contention while leaving the run finite.
    constexpr int kPushesPerThread = 400;
    constexpr int kSenderThreads = 4;

    std::atomic<int> pushed{0};

    std::vector<std::thread> senders;
    senders.reserve(kSenderThreads);
    for (int t = 0; t < kSenderThreads; ++t)
    {
        senders.emplace_back(
            [&scenario, &pushed, t]()
            {
                for (int i = 0; i < kPushesPerThread; ++i)
                {
                    const Entity player = scenario.Players()[static_cast<std::size_t>(t) % scenario.Players().size()];
                    const std::uint32_t roll = Hash(static_cast<std::uint32_t>(t), static_cast<std::uint32_t>(i));
                    const int direction = static_cast<int>(roll % 9);

                    scenario.Sim().Commands().Push(MoveCommand{player, (direction % 3) - 1, (direction / 3) - 1});
                    pushed.fetch_add(1);
                    std::this_thread::yield();
                }
            });
    }

    for (int tick = 0; tick < 150; ++tick)
    {
        scenario.Step(tick);
    }

    for (std::thread& sender : senders)
    {
        sender.join();
    }

    // Drain whatever landed after the last tick.
    scenario.Sim().Tick(kTickSeconds);

    CHECK_EQ(scenario.Failures(), 0);
    CHECK_EQ(pushed.load(), kSenderThreads * kPushesPerThread);
    CHECK_EQ(scenario.Sim().Commands().PendingCount(), 0u);
    CHECK_EQ(scenario.Sim().Commands().UnhandledCount(), 0u);
}

} // namespace

int main()
{
    TheWorldStaysConsistentUnderLoad();
    TwoIdenticalRunsAgreeExactly();
    APlayerDyingMidRunIsHandledCleanly();
    CommandsFromManyThreadsSurviveALiveSimulation();

    return world_v2::test::Summary("Scenario");
}
