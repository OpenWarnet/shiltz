// Measures the three performance claims world_v2 was built on, each
// against the alternative it was chosen over:
//
//   1. Sparse-set storage beats a hash map for both iteration and random
//      access -- the cache-locality goal.
//   2. A smallest-pool-driven View beats the brute-force one in the design
//      document, which scanned every entity ever created.
//   3. Sequential component type ids beat std::type_index + unordered_map
//      for pool lookup, which happens on every Has/Get.
//
// Plus a realistic full-tick cost, so the tick budget is a number rather
// than a guess.
//
// Release builds only. Under debug MSVC, container instrumentation costs
// more than anything measured here and every comparison collapses.

#include "Simulation.h"
#include "component/Combat.h"
#include "component/Grid.h"
#include "component/Items.h"
#include "core/Benchmark.h"
#include "core/ComponentPool.h"
#include "core/Entity.h"
#include "core/Registry.h"
#include "core/TypeId.h"
#include "component/Network.h"
#include "event/CombatEvents.h"
#include "event/MovementEvents.h"
#include "system/AISystem.h"
#include "system/BroadcastSystem.h"
#include "system/CombatRules.h"
#include "world/MapWorld.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <typeindex>
#include <unordered_map>
#include <vector>

using namespace world_v2;
using namespace world_v2::bench;

namespace
{

constexpr int kRepeats = 15;

struct Position
{
    int x = 0;
    int y = 0;
};

struct Velocity
{
    int dx = 0;
    int dy = 0;
};

struct Health
{
    int current = 0;
    int max = 0;
};

// A registry filled with `count` entities, every one holding a Position and
// every `velocityEvery`-th also holding a Velocity.
//
// Nothing is destroyed while filling, so every handle has generation 0 and
// an entity's handle equals its index. The brute-force view below relies on
// that to rebuild handles from a counter, exactly as the design document's
// version did.
struct Fixture
{
    Registry registry;
    std::vector<Entity> entities;

    Fixture(std::size_t count, std::size_t velocityEvery)
    {
        entities.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const Entity entity = registry.Create();
            registry.Assign<Position>(entity, static_cast<int>(i), static_cast<int>(i));

            if (velocityEvery != 0 && i % velocityEvery == 0)
            {
                registry.Assign<Velocity>(entity, 1, 0);
            }

            entities.push_back(entity);
        }
    }
};

std::unordered_map<Entity, Position> BuildMap(std::size_t count)
{
    std::unordered_map<Entity, Position> map;
    map.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        map.emplace(static_cast<Entity>(i), Position{static_cast<int>(i), static_cast<int>(i)});
    }
    return map;
}

// The View from section 5.2 of the design document: walk every entity id
// ever handed out, collect the matches into a vector, then iterate that.
std::uint64_t BruteForceView(Registry& registry, std::size_t entityCount)
{
    std::vector<Entity> matches;
    for (std::size_t i = 0; i < entityCount; ++i)
    {
        const Entity entity = static_cast<Entity>(i);
        if (registry.Has<Position>(entity) && registry.Has<Velocity>(entity))
        {
            matches.push_back(entity);
        }
    }

    std::uint64_t checksum = 0;
    for (const Entity entity : matches)
    {
        checksum += static_cast<std::uint64_t>(registry.Get<Position>(entity).x);
        checksum += static_cast<std::uint64_t>(registry.Get<Velocity>(entity).dx);
    }
    return checksum;
}

void IterationSingleComponent(std::size_t count)
{
    Fixture fixture(count, 0);
    const std::unordered_map<Entity, Position> map = BuildMap(count);

    // A bare pool, walked directly, with no filtering and no entity
    // indirection at all. This is the floor everything else is measured
    // against -- not something the framework offers as an API.
    ComponentPool<Position> bare;
    for (std::size_t i = 0; i < count; ++i)
    {
        bare.Emplace(MakeEntity(static_cast<std::uint32_t>(i), 0), static_cast<int>(i), static_cast<int>(i));
    }

    Header("1. Iteration, one component (100% match)");

    Row("dense array walk (the floor)", count,
        Measure(kRepeats,
                [&]()
                {
                    std::uint64_t checksum = 0;
                    for (const Position& position : bare.Raw())
                    {
                        checksum += static_cast<std::uint64_t>(position.x);
                    }
                    return checksum;
                }));

    Row("Registry::view<Position>().Each", count,
        Measure(kRepeats,
                [&]()
                {
                    std::uint64_t checksum = 0;
                    fixture.registry.view<Position>().Each(
                        [&checksum](Entity, Position& position)
                        { checksum += static_cast<std::uint64_t>(position.x); });
                    return checksum;
                }));

    Row("unordered_map<Entity, Position> walk", count,
        Measure(kRepeats,
                [&]()
                {
                    std::uint64_t checksum = 0;
                    for (const auto& entry : map)
                    {
                        checksum += static_cast<std::uint64_t>(entry.second.x);
                    }
                    return checksum;
                }));
}

void IterationTwoComponents(std::size_t count, std::size_t velocityEvery, const char* section)
{
    Fixture fixture(count, velocityEvery);
    const std::size_t matching = fixture.registry.Count<Velocity>();

    Header(section);

    Row("smallest-pool View (world_v2)", matching,
        Measure(kRepeats,
                [&]()
                {
                    std::uint64_t checksum = 0;
                    fixture.registry.view<Position, Velocity>().Each(
                        [&checksum](Entity, Position& position, Velocity& velocity)
                        {
                            checksum += static_cast<std::uint64_t>(position.x);
                            checksum += static_cast<std::uint64_t>(velocity.dx);
                        });
                    return checksum;
                }));

    Row("brute-force View (design doc 5.2)", matching,
        Measure(kRepeats, [&]() { return BruteForceView(fixture.registry, count); }));

    std::printf("  -- %zu of %zu entities match; ns/op is per *matching* entity\n", matching, count);
}

void RandomAccess(std::size_t count, std::size_t lookups)
{
    Fixture fixture(count, 0);
    const std::unordered_map<Entity, Position> map = BuildMap(count);

    // A fixed shuffle, so both structures are probed in the same
    // cache-hostile order rather than walking straight through memory.
    std::vector<Entity> order;
    order.reserve(lookups);
    std::mt19937 rng(1234);
    std::uniform_int_distribution<std::size_t> pick(0, count - 1);
    for (std::size_t i = 0; i < lookups; ++i)
    {
        order.push_back(fixture.entities[pick(rng)]);
    }

    Header("3. Random access by handle");

    Row("Registry::Get<Position> (sparse set)", lookups,
        Measure(kRepeats,
                [&]()
                {
                    std::uint64_t checksum = 0;
                    for (const Entity entity : order)
                    {
                        checksum += static_cast<std::uint64_t>(fixture.registry.Get<Position>(entity).x);
                    }
                    return checksum;
                }));

    Row("unordered_map::at", lookups,
        Measure(kRepeats,
                [&]()
                {
                    std::uint64_t checksum = 0;
                    for (const Entity entity : order)
                    {
                        checksum += static_cast<std::uint64_t>(map.at(entity).x);
                    }
                    return checksum;
                }));
}

// The pool-lookup step on its own, isolated from everything else a Has/Get
// does, so the type-id decision can be measured rather than argued.
void TypeLookup(std::size_t iterations)
{
    // Sized and filled by the real type ids, so the indexing is exactly
    // what Registry does rather than a hardcoded slot.
    const TypeId positionId = TypeIdOf<ComponentFamily>::Value<Position>();
    const TypeId velocityId = TypeIdOf<ComponentFamily>::Value<Velocity>();
    const TypeId healthId = TypeIdOf<ComponentFamily>::Value<Health>();

    std::vector<std::unique_ptr<IComponentPool>> byIndex;
    byIndex.resize(static_cast<std::size_t>(std::max(std::max(positionId, velocityId), healthId)) + 1);
    byIndex[positionId] = std::make_unique<ComponentPool<Position>>();
    byIndex[velocityId] = std::make_unique<ComponentPool<Velocity>>();
    byIndex[healthId] = std::make_unique<ComponentPool<Health>>();

    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> byTypeIndex;
    byTypeIndex.emplace(std::type_index(typeid(Position)), std::make_unique<ComponentPool<Position>>());
    byTypeIndex.emplace(std::type_index(typeid(Velocity)), std::make_unique<ComponentPool<Velocity>>());
    byTypeIndex.emplace(std::type_index(typeid(Health)), std::make_unique<ComponentPool<Health>>());

    Header("4. Component pool lookup (the step every Has/Get pays)");

    Row("vector index by sequential TypeId (world_v2)", iterations,
        Measure(kRepeats,
                [&]()
                {
                    std::uint64_t checksum = 0;
                    for (std::size_t i = 0; i < iterations; ++i)
                    {
                        const TypeId id = TypeIdOf<ComponentFamily>::Value<Position>();
                        checksum += byIndex[id]->Size() + id;
                    }
                    return checksum;
                }));

    Row("unordered_map<type_index> (design doc 5.2)", iterations,
        Measure(kRepeats,
                [&]()
                {
                    std::uint64_t checksum = 0;
                    for (std::size_t i = 0; i < iterations; ++i)
                    {
                        const auto found = byTypeIndex.find(std::type_index(typeid(Position)));
                        checksum += found->second->Size() + 1;
                    }
                    return checksum;
                }));
}

// The inbound queue on its own. The full-tick number below includes this,
// and each Push heap-allocates a type-erased wrapper -- a cost flagged when
// CommandQueue was written but never until now measured, so this separates
// it out rather than leaving the tick figure ambiguous.
void CommandOverhead(std::size_t count)
{
    struct NoopCommand
    {
        Entity entity = kNullEntity;
    };

    MapWorld world(8, 8, true);
    CommandQueue queue;
    queue.On<NoopCommand>([](MapWorld&, const NoopCommand&) {});

    Header("5. Command queue overhead (included in the tick below)");

    Row("Push x N then Drain", count,
        Measure(kRepeats,
                [&]()
                {
                    for (std::size_t i = 0; i < count; ++i)
                    {
                        queue.Push(NoopCommand{});
                    }
                    return static_cast<std::uint64_t>(queue.Drain(world));
                }));
}

// End to end: a populated map, a fraction of it moving, one Simulation tick.
void FullTick(std::size_t creatures, std::size_t moversEvery)
{
    struct MoveCommand
    {
        Entity entity = kNullEntity;
    };

    Simulation simulation(512, 512, true);
    simulation.Commands().On<MoveCommand>(
        [](MapWorld& world, const MoveCommand& command)
        {
            if (world.registry.Exists(command.entity))
            {
                world.registry.Assign<MoveIntentComponent>(command.entity, 1, 0);
            }
        });

    // Spread two tiles apart across the grid.
    std::vector<Entity> movers;
    std::size_t placed = 0;
    for (std::size_t i = 0; placed < creatures; ++i)
    {
        const int x = static_cast<int>((i * 2) % 512);
        const int y = static_cast<int>((i * 2) / 512);
        if (y >= 512)
        {
            break;
        }

        const Entity entity = simulation.World().SpawnBlocking(x, y);
        if (entity == kNullEntity)
        {
            continue;
        }

        ++placed;
        if (moversEvery != 0 && placed % moversEvery == 0)
        {
            movers.push_back(entity);
        }
    }

    Header("6. Full Simulation::Tick");

    // At 4 tiles/sec a 0.25s step completes within the tick, so every tick
    // starts with the movers free to step again. After the first couple of
    // repeats most of them are pressed up against a neighbour, so this is a
    // mix of accepted and rejected steps rather than a best case.
    Row("tick (ns/op is per moving creature)", movers.size(),
        Measure(kRepeats,
                [&]()
                {
                    for (const Entity entity : movers)
                    {
                        simulation.Commands().Push(MoveCommand{entity});
                    }
                    simulation.Tick(0.25f);
                    return static_cast<std::uint64_t>(simulation.LastCommandCount());
                }));

    std::printf("  -- %zu creatures on the map, %zu of them moving each tick\n", placed, movers.size());
    std::printf("  -- the us column is the whole-tick cost; note it tracks movers, not map population\n");
}


// --- Combat scenarios -------------------------------------------------------
//
// The tick benchmark above deliberately holds no health or AI components, so
// AI, combat and death sweep empty pools there and it says nothing about
// them. These build worlds those systems actually have work to do in.

constexpr int kMonsterFaction = 1;
constexpr int kPlayerFaction = 2;

// A field of monsters, optionally with something to fight.
//
// Having nothing to fight is the *worst* case for vision: a monster with no
// valid target re-scans its whole square every tick, where one that already
// holds a target validates it and skips the search entirely.
struct AiScene
{
    MapWorld world;
    std::size_t monsters = 0;

    AiScene(int size, std::size_t monsterCount, int visionRange, std::size_t distantCount, bool giveEachATarget)
        : world(size, size, true)
    {
        for (std::size_t i = 0; i < monsterCount; ++i)
        {
            const int x = static_cast<int>((i % 40) * 8);
            const int y = static_cast<int>((i / 40) * 8);

            const Entity monster = world.SpawnBlocking(x, y);
            if (monster == kNullEntity)
            {
                continue;
            }

            world.registry.Assign<FactionComponent>(monster, kMonsterFaction);
            world.registry.Assign<AIComponent>(monster, visionRange, 1, kNullEntity);
            world.registry.Assign<HealthComponent>(monster, 100, 100);
            ++monsters;

            if (giveEachATarget)
            {
                const Entity prey = world.SpawnBlocking(x + 1, y);
                if (prey != kNullEntity)
                {
                    world.registry.Assign<FactionComponent>(prey, kPlayerFaction);
                    world.registry.Assign<HealthComponent>(prey, 1000000, 1000000);
                }
            }
        }

        // Bystanders parked far past any vision square, to show the scan
        // cost does not move when the map fills up.
        for (std::size_t i = 0; i < distantCount; ++i)
        {
            const int x = static_cast<int>(i % 256);
            const int y = 300 + static_cast<int>(i / 256);

            const Entity extra = world.SpawnBlocking(x, y);
            if (extra != kNullEntity)
            {
                world.registry.Assign<FactionComponent>(extra, kPlayerFaction);
                world.registry.Assign<HealthComponent>(extra, 100, 100);
            }
        }
    }
};

void AiVisionScan()
{
    Header("7. AI target search (ns/op is per monster)");

    {
        AiScene sparse(512, 1000, 4, 0, false);
        AISystem ai;
        Row("vision 4, nothing to find, empty map", sparse.monsters,
            Measure(kRepeats,
                    [&]()
                    {
                        ai.Update(sparse.world.registry, sparse.world.tiles);
                        return static_cast<std::uint64_t>(sparse.monsters);
                    }));
    }

    {
        AiScene crowded(512, 1000, 4, 8000, false);
        AISystem ai;
        Row("vision 4, nothing to find, +8000 elsewhere", crowded.monsters,
            Measure(kRepeats,
                    [&]()
                    {
                        ai.Update(crowded.world.registry, crowded.world.tiles);
                        return static_cast<std::uint64_t>(crowded.monsters);
                    }));
    }

    {
        AiScene engaged(512, 1000, 4, 0, true);
        AISystem ai;
        ai.Update(engaged.world.registry, engaged.world.tiles);

        Row("vision 4, already engaged (no search)", engaged.monsters,
            Measure(kRepeats,
                    [&]()
                    {
                        ai.Update(engaged.world.registry, engaged.world.tiles);
                        return static_cast<std::uint64_t>(engaged.monsters);
                    }));
    }

    Note("the first two should match: scan cost is bounded by vision, not by population");
}

void AiVisionRangeScaling()
{
    Header("8. AI target search by vision range (ns/op is per monster)");

    const int ranges[] = {2, 4, 8, 16};
    const char* labels[] = {"vision 2  (5x5 tiles scanned)", "vision 4  (9x9 tiles scanned)",
                            "vision 8  (17x17 tiles scanned)", "vision 16 (33x33 tiles scanned)"};

    for (int i = 0; i < 4; ++i)
    {
        AiScene scene(512, 1000, ranges[i], 0, false);
        AISystem ai;
        Row(labels[i], scene.monsters,
            Measure(kRepeats,
                    [&]()
                    {
                        ai.Update(scene.world.registry, scene.world.tiles);
                        return static_cast<std::uint64_t>(scene.monsters);
                    }));
    }

    Note("quadratic in range -- this is what a vision number in game data costs");
}

void CombatSteadyState()
{
    Simulation simulation(256, 256, true);
    InstallCombatRules(simulation.World());

    // Clusters of monsters in melee with a player they cannot kill, so every
    // tick runs a full AI pass, movement, and combat resolution without the
    // population changing underneath the measurement.
    std::size_t monsters = 0;
    std::size_t players = 0;

    for (int p = 0; p < 200; ++p)
    {
        const int px = (p % 20) * 12 + 6;
        const int py = (p / 20) * 24 + 12;

        const Entity player = simulation.World().SpawnBlocking(px, py);
        if (player == kNullEntity)
        {
            continue;
        }

        simulation.World().registry.Assign<FactionComponent>(player, kPlayerFaction);
        simulation.World().registry.Assign<HealthComponent>(player, 1000000000, 1000000000);
        ++players;

        int placed = 0;
        for (int dy = -2; dy <= 2 && placed < 10; ++dy)
        {
            for (int dx = -2; dx <= 2 && placed < 10; ++dx)
            {
                if (dx == 0 && dy == 0)
                {
                    continue;
                }

                const Entity monster = simulation.World().SpawnBlocking(px + dx, py + dy);
                if (monster == kNullEntity)
                {
                    continue;
                }

                simulation.World().registry.Assign<FactionComponent>(monster, kMonsterFaction);
                simulation.World().registry.Assign<AIComponent>(monster, 6, 1, kNullEntity);
                simulation.World().registry.Assign<HealthComponent>(monster, 100, 100);
                simulation.World().registry.Assign<AttackPowerComponent>(monster, 1);
                ++monsters;
                ++placed;
            }
        }
    }

    // Let the approach resolve into melee first, so this times the steady
    // state rather than the chase.
    for (int i = 0; i < 10; ++i)
    {
        simulation.Tick(0.25f);
    }

    Header("9. Full tick with combat (ns/op is per monster)");

    Row("AI + movement + combat + death", monsters,
        Measure(kRepeats,
                [&]()
                {
                    simulation.Tick(0.25f);
                    return static_cast<std::uint64_t>(simulation.World().registry.Count<AttackRequestComponent>());
                }));

    std::printf("  -- %zu monsters engaged with %zu players, all in melee\n", monsters, players);
}

void DeathCascade(std::size_t victims)
{
    // Consumes what it runs on: every victim is despawned by the barrier, so
    // the world is rebuilt between runs, outside the timed region.
    std::unique_ptr<Simulation> simulation;

    const auto setup = [&]()
    {
        simulation = std::make_unique<Simulation>(512, 512, true);
        InstallCombatRules(simulation->World());

        for (std::size_t i = 0; i < victims; ++i)
        {
            const int x = static_cast<int>(i % 400);
            const int y = static_cast<int>(i / 400);

            const Entity victim = simulation->World().SpawnBlocking(x, y);
            if (victim == kNullEntity)
            {
                continue;
            }

            simulation->World().registry.Assign<FactionComponent>(victim, kMonsterFaction);
            simulation->World().registry.Assign<ExperienceRewardComponent>(victim, std::uint64_t{10});
            simulation->World().registry.Assign<LootTableComponent>(victim, std::uint32_t{7});

            // Already at zero: the tick below is the one that notices.
            simulation->World().registry.Assign<HealthComponent>(victim, 0, 100);
        }
    };

    Header("10. Death cascade in one barrier (ns/op is per death)");

    Row("announce, credit, drop, despawn", victims,
        MeasureWithSetup(kRepeats, setup,
                         [&]()
                         {
                             simulation->Tick(0.0f);
                             return static_cast<std::uint64_t>(simulation->World().registry.AliveCount());
                         }));

    Note("every one is a structural removal, all inside a single Flush");
}


// Stage 4's filter is a plain pass over viewers against notices --
// O(viewers x notices). That is the obvious implementation rather than the
// clever one, on the grounds that a zone index would be complexity bought on
// a guess. This is the measurement that guess was deferred to.
void BroadcastDelivery(std::size_t viewerCount, std::size_t noticeCount)
{
    MapWorld world(512, 512, true);
    BroadcastSystem broadcast;
    broadcast.Install(world);

    // Viewers spread across the map, each seeing a 25x25 window.
    std::size_t viewers = 0;
    for (std::size_t i = 0; i < viewerCount; ++i)
    {
        const int x = static_cast<int>((i % 32) * 16);
        const int y = static_cast<int>((i / 32) * 16);

        const Entity viewer = world.SpawnBlocking(x, y);
        if (viewer == kNullEntity)
        {
            continue;
        }

        world.registry.Assign<ViewerComponent>(viewer, 12);
        ++viewers;
    }

    std::size_t delivered = 0;
    const NoticeSink sink = [&delivered](Entity, const Notice&) { ++delivered; };
    const NoticeSink noSink;

    // Each run emits a tick's worth of movement, flushes it into notices,
    // then delivers. Running it once with a sink and once without isolates
    // the filtering from the collection.
    const auto produce = [&]()
    {
        for (std::size_t i = 0; i < noticeCount; ++i)
        {
            const int x = static_cast<int>(i % 512);
            const int y = static_cast<int>((i / 512) % 512);
            world.events.Emit(EntityMovedEvent{kNullEntity, x, y, x + 1, y, 4.0f});
        }
        world.events.Flush();
    };

    Header("11. Stage 4 broadcast (ns/op is per viewer-notice pair)");

    Row("collect only (no sink)", viewers * noticeCount,
        Measure(kRepeats,
                [&]()
                {
                    produce();
                    broadcast.Deliver(world.registry, noSink);
                    return static_cast<std::uint64_t>(noticeCount);
                }));

    Row("collect + filter over every viewer", viewers * noticeCount,
        Measure(kRepeats,
                [&]()
                {
                    delivered = 0;
                    produce();
                    broadcast.Deliver(world.registry, sink);
                    return static_cast<std::uint64_t>(delivered);
                }));

    std::printf("  -- %zu viewers x %zu notices = %zu pairs examined; %zu passed the filter\n", viewers, noticeCount,
                viewers * noticeCount, delivered);
    std::printf("  -- the difference between the two rows is the filtering cost\n");
}

} // namespace

int main()
{
    std::printf("world_v2 benchmarks -- best of %d runs\n", kRepeats);
    std::printf("(meaningful only in a release build)\n");

    IterationSingleComponent(100000);
    IterationTwoComponents(100000, 100, "2a. Iteration, two components (1% match -- the common case)");
    IterationTwoComponents(100000, 1, "2b. Iteration, two components (100% match -- worst case for us)");
    RandomAccess(100000, 100000);
    TypeLookup(1000000);
    CommandOverhead(500);
    FullTick(5000, 10);
    AiVisionScan();
    AiVisionRangeScaling();
    CombatSteadyState();
    DeathCascade(2000);
    BroadcastDelivery(200, 2000);

    std::printf("\n");
    return 0;
}
