// A world that runs with nothing attached to it.
//
// No GameServer, no sockets, no protocol. This builds three simulations
// under one World, invents some clients, and drives the whole thing at 20Hz
// against a real clock, printing what a network layer would otherwise be
// serializing. It is how you watch the simulation work before there is
// anything to watch it with.
//
//     world_v2_runner [seconds] [--quiet]
//
//       seconds   how long to run (default 15)
//       --quiet   summaries only, no per-notice lines
//
// Not registered with ctest -- it sleeps in real time and prints, which is
// the opposite of what a test should do. World.test.cpp is where the claims
// are checked; this is where they are watched.
//
// The fake clients here only ever act on notices they were sent. They are
// not allowed to read the registry, because a real client cannot, and a
// driver that cheated would prove the simulation works while proving
// nothing about whether the outbound half is usable. The one shortcut is
// that their commands name entities by Entity handle rather than by
// NetworkIDComponent id -- a real game layer must translate, since
// Network.h is explicit that a storage slot never goes on the wire.

#include "World.h"

#include "component/Combat.h"
#include "component/Grid.h"
#include "component/Items.h"
#include "component/Network.h"
#include "component/Request.h"
#include "component/Spawn.h"
#include "core/Entity.h"
#include "event/CombatEvents.h"
#include "event/SpawnEvents.h"
#include "system/BroadcastModule.h"
#include "system/CombatRules.h"
#include "system/CoreSimulationModule.h"
#include "system/DespawnRules.h"
#include "system/ItemRules.h"
#include "system/SpawnRules.h"
#include "world/Inventory.h"
#include "core/Map.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace world_v2;

namespace
{

constexpr int kMapSize = 32;
constexpr int kSimulationCount = 3;
constexpr int kClientCount = 9;

constexpr float kTickSeconds = 0.05f;
constexpr int kTicksPerSecond = 20;

constexpr int kViewRadius = 8;
constexpr int kPlayerHealth = 60;
constexpr int kPlayerDamage = 9;
constexpr int kPlayerFaction = 1;
constexpr int kMonsterFaction = 2;

constexpr int kSpawnersPerMap = 3;
constexpr int kMonstersPerSpawner = 3;

// Template ids the clients use to tell what they are looking at. Monsters
// come from the spawners, players are announced with their own character id
// offset into a range of their own, so a client can recognise itself in the
// spawn notice it receives.
constexpr std::uint32_t kMonsterTemplateBase = 500;
constexpr std::uint32_t kPlayerTemplateBase = 9000;

// Seconds unclaimed loot stays on the floor.
constexpr float kLootLifetime = 12.0f;

// Ticks between a client's warp attempts, and how long it waits to rejoin
// after being dropped.
constexpr int kWarpInterval = 60;
constexpr int kRejoinDelay = 20;

// --------------------------------------------------------------------
// Deterministic noise. A runner that wandered differently every time would
// be useless for comparing two runs, so nothing here touches <random>.
struct Rng
{
    std::uint32_t state = 0x2545f491u;

    std::uint32_t Next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    int Range(int count)
    {
        return count <= 0 ? 0 : static_cast<int>(Next() % static_cast<std::uint32_t>(count));
    }
};

// --------------------------------------------------------------------
// What a client can ask for. Each carries a connection and nothing that
// identifies the sender's entity -- see the PlayerCommand concept.

struct StepCommand
{
    ConnectionId connection = kInvalidConnection;
    int directionX = 0;
    int directionY = 0;
};

struct AttackCommand
{
    ConnectionId connection = kInvalidConnection;
    Entity target = kNullEntity;
    int damage = 0;
};

struct PickupCommand
{
    ConnectionId connection = kInvalidConnection;
    Entity item = kNullEntity;
};

// --------------------------------------------------------------------

// One thing a client has been told about and has not been told to forget.
struct Sighting
{
    int x = 0;
    int y = 0;
    std::uint32_t templateId = 0;
    bool item = false;
};

// A client, as far as it can tell from what it has received.
//
// Everything here is built from notices. There is deliberately no pointer
// to a Simulation, no Entity lookup, and no way to ask where anything is --
// if the outbound stream does not say it, this does not know it.
struct FakeClient
{
    ConnectionId connection = kInvalidConnection;
    CharacterId character = kInvalidCharacter;
    SimulationId simulation = kInvalidSimulationId;

    Entity self = kNullEntity;
    int x = 0;
    int y = 0;
    bool onMap = false;

    std::unordered_map<Entity, Sighting> seen;

    int warpTimer = 0;
    int rejoinTimer = -1;

    int kills = 0;
    int deaths = 0;
    int pickups = 0;

    std::uint32_t OwnTemplateId() const
    {
        return kPlayerTemplateBase + character;
    }

    void Forget()
    {
        seen.clear();
        self = kNullEntity;
        onMap = false;
    }
};

// --------------------------------------------------------------------

const char* KindName(NoticeKind kind)
{
    switch (kind)
    {
    case NoticeKind::Spawned:
        return "Spawned";
    case NoticeKind::Moved:
        return "Moved";
    case NoticeKind::Damaged:
        return "Damaged";
    case NoticeKind::Died:
        return "Died";
    case NoticeKind::Despawned:
        return "Despawned";
    case NoticeKind::ItemAppeared:
        return "Item";
    }

    return "?";
}

// The game layer's half of a map: what a player is, what a monster is, what
// a corpse leaves behind, and what each command does.
//
// This is everything world_v2 refuses to decide. It used to be one function
// calling six different kinds of wiring in an order that mattered and was
// written down nowhere. Now each piece is a module, and the order that
// matters is the install list at the bottom of this file.

// The seam world_v2 leaves open on purpose: turning a drop-table id into
// items needs .scr data the framework does not read. One item per corpse
// here, with a timer so an uncollected floor does not grow without bound.
class LootModule : public Module
{
public:
    const char* Name() const override
    {
        return "Loot";
    }

    void Setup(ModuleContext& context) override
    {
        Map& world = context.World();

        context.Listen<LootDropEvent>([&world](const LootDropEvent& event) {
            const Entity item = SpawnGroundItem(world, event.dropTableId, 1, 10, event.x, event.y);
            if (item != kNullEntity)
            {
                world.registry.Assign<DespawnTimerComponent>(item, kLootLifetime);
            }
        });
    }
};

// What a monster is, once a template id has been turned into an entity.
MonsterFactory MakeMonsterFactory()
{
    return [](Map& map, Entity monster, std::uint32_t templateId) {
        map.registry.Assign<FactionComponent>(monster, kMonsterFaction);
        map.registry.Assign<HealthComponent>(monster, 20, 20);
        map.registry.Assign<AIComponent>(monster, 7, 1, kNullEntity);
        map.registry.Assign<AttackPowerComponent>(monster, 3);
        map.registry.Assign<ExperienceRewardComponent>(monster, std::uint64_t{35});
        map.registry.Assign<LootTableComponent>(monster, templateId);
    };
}

// What a player is, and what each of the three client commands does.
class PlayerModule : public Module
{
public:
    const char* Name() const override
    {
        return "Player";
    }

    void Setup(ModuleContext& context) override
    {
        context.OnSpawnPlayer([](Map& map, const JoinCommand& command) -> Entity {
            const Entity player = map.Spawn(command.x, command.y);
            if (player == kNullEntity || !map.tiles.IsWalkable(command.x, command.y))
            {
                if (player != kNullEntity)
                {
                    map.Despawn(player);
                }

                return kNullEntity;
            }

            map.registry.Assign<ViewerComponent>(player, kViewRadius);
            map.registry.Assign<HealthComponent>(player, kPlayerHealth, kPlayerHealth);
            map.registry.Assign<FactionComponent>(player, kPlayerFaction);
            map.registry.Assign<ExperienceComponent>(player, std::uint64_t{0}, std::uint32_t{1}, std::uint64_t{100});
            map.registry.Assign<InventoryComponent>(player, MakeInventory(8));
            map.registry.Assign<NetworkIDComponent>(player, static_cast<std::uint32_t>(command.connection));

            // Announced like anything else that appears, which is what lets
            // other players see the newcomer -- and what lets the newcomer
            // recognise itself, since the template id carries its character.
            map.events.Emit(MonsterSpawnedEvent{player, kNullEntity, kPlayerTemplateBase + command.character,
                                                command.x, command.y});

            return player;
        });

        context.OnPlayerCommand<StepCommand>([](Map& map, Entity actor, const StepCommand& command) {
            map.registry.Assign<MoveIntentComponent>(actor, command.directionX, command.directionY);
        });

        context.OnPlayerCommand<AttackCommand>([](Map& map, Entity actor, const AttackCommand& command) {
            // The target may have died between the client deciding and this
            // draining -- the generation in the handle is what makes that a
            // rejected command rather than a hit on whoever took the slot.
            if (!map.registry.Exists(command.target))
            {
                return;
            }

            map.registry.Assign<AttackRequestComponent>(actor, command.target, command.damage);
        });

        context.OnPlayerCommand<PickupCommand>([](Map& map, Entity actor, const PickupCommand& command) {
            if (!map.registry.Exists(command.item))
            {
                return;
            }

            map.registry.Assign<PickupItemRequestComponent>(actor, command.item);
        });
    }
};

// A few walls, so paths are not all open field and the AI has something to
// walk around. Deterministic, and the same on every map.
void CarveTerrain(Map& world)
{
    for (int y = 6; y < kMapSize - 6; ++y)
    {
        world.tiles.SetWalkable(kMapSize / 2, y, false);
    }

    // A doorway, so the two halves are not sealed off from each other.
    world.tiles.SetWalkable(kMapSize / 2, kMapSize / 2, true);

    for (int x = 4; x < 12; ++x)
    {
        world.tiles.SetWalkable(x, kMapSize - 8, false);
    }
}

void PlaceSpawners(Map& world, SimulationId id)
{
    for (int index = 0; index < kSpawnersPerMap; ++index)
    {
        const int x = 6 + index * 9;
        const int y = 6 + ((index + id) % 3) * 8;

        const Entity spawner = world.registry.Create();
        world.registry.Assign<SpawnerComponent>(spawner, kMonsterTemplateBase + static_cast<std::uint32_t>(index), x, y,
                                                3, kMonstersPerSpawner, 0, 4.0f, 0.0f, std::uint32_t{0});
    }
}

// World data rather than behaviour, but it belongs in the install list for
// the same reason everything else does: what a map is made of should be one
// readable sequence.
//
// Both halves run during Setup, which is what lets SpawnRulesModule::Start
// prime these spawners afterwards -- the ordering that used to be three
// statements the caller had to keep in the right sequence.
class TerrainModule : public Module
{
public:
    explicit TerrainModule(SimulationId id)
        : m_id(id)
    {
    }

    const char* Name() const override
    {
        return "Terrain";
    }

    void Setup(ModuleContext& context) override
    {
        CarveTerrain(context.World());
        PlaceSpawners(context.World(), m_id);
    }

private:
    SimulationId m_id;
};

// --------------------------------------------------------------------

class Runner
{
public:
    Runner(int totalTicks, bool quiet)
        : m_totalTicks(totalTicks)
        , m_quiet(quiet)
    {
        for (int index = 0; index < kSimulationCount; ++index)
        {
            const SimulationId id = static_cast<SimulationId>(index + 1);
            Simulation& simulation = m_world.Create(id, kMapSize, kMapSize, true);

            // The whole of what this map is, top to bottom. Stage-2
            // systems run in this order; everything else is a listener,
            // a command handler, or world data.
            simulation.Install<CoreSimulationModule>();
            simulation.Install<BroadcastModule>();
            simulation.Install<CombatRulesModule>();
            simulation.Install<SpawnRulesModule>(MakeMonsterFactory());
            simulation.Install<ItemRulesModule>();
            simulation.Install<DespawnRulesModule>();
            simulation.Install<TerrainModule>(id);
            simulation.Install<LootModule>();
            simulation.Install<PlayerModule>();

            // Runs every module's Start -- which is where the spawners
            // this map just authored get primed -- and seals the
            // simulation. The first Tick would do it anyway; doing it here
            // means the world is populated before the clients join.
            simulation.Start();
        }

        m_world.OnNotice([this](ConnectionId connection, SimulationId simulation, const Notice& notice) {
            OnNotice(connection, simulation, notice);
        });

        m_world.OnConnectionDropped([this](ConnectionId connection, SimulationId simulation) {
            OnDropped(connection, simulation);
        });

        for (int index = 0; index < kClientCount; ++index)
        {
            FakeClient client;
            client.connection = static_cast<ConnectionId>(index + 1);
            client.character = static_cast<CharacterId>(1000 + index);
            client.simulation = static_cast<SimulationId>((index % kSimulationCount) + 1);
            client.warpTimer = kWarpInterval + index * 7;
            m_clients.push_back(client);
        }

        for (FakeClient& client : m_clients)
        {
            Join(client);
        }
    }

    void Run()
    {
        using Clock = std::chrono::steady_clock;
        const auto period = std::chrono::microseconds(1000000 / kTicksPerSecond);
        auto next = Clock::now();

        for (m_tick = 0; m_tick < m_totalTicks; ++m_tick)
        {
            m_world.Tick(kTickSeconds);
            DriveClients();

            if ((m_tick + 1) % kTicksPerSecond == 0)
            {
                PrintSummary();
            }

            next += period;
            std::this_thread::sleep_until(next);
        }

        PrintTotals();
    }

private:
    float Now() const
    {
        return static_cast<float>(m_tick) * kTickSeconds;
    }

    FakeClient* ClientFor(ConnectionId connection)
    {
        for (FakeClient& client : m_clients)
        {
            if (client.connection == connection)
            {
                return &client;
            }
        }

        return nullptr;
    }

    void Join(FakeClient& client)
    {
        // A fixed spread of starting tiles, all in open ground.
        const int x = 3 + static_cast<int>(client.character % 7) * 3;
        const int y = 3 + static_cast<int>(client.character % 5) * 3;

        client.Forget();
        if (!m_world.Join(client.connection, client.character, client.simulation, x, y))
        {
            std::printf("[t=%6.2f] conn=%llu  JOIN REFUSED by World\n", Now(),
                        static_cast<unsigned long long>(client.connection));
        }
    }

    // The outbound half, from the point of view of something that has to
    // turn it into state. This is the closest thing here to what a real
    // network layer does, minus the serializing.
    void OnNotice(ConnectionId connection, SimulationId simulation, const Notice& notice)
    {
        ++m_noticeCount;

        FakeClient* client = ClientFor(connection);
        if (client == nullptr)
        {
            return;
        }

        switch (notice.kind)
        {
        case NoticeKind::Spawned:
            if (notice.templateId == client->OwnTemplateId())
            {
                client->self = notice.subject;
                client->x = notice.x;
                client->y = notice.y;
                client->onMap = true;
                client->simulation = simulation;
            }
            else
            {
                client->seen[notice.subject] = Sighting{notice.x, notice.y, notice.templateId, false};
            }
            break;

        case NoticeKind::Moved:
            if (notice.subject == client->self)
            {
                client->x = notice.x;
                client->y = notice.y;
            }
            else
            {
                Sighting& sighting = client->seen[notice.subject];
                sighting.x = notice.x;
                sighting.y = notice.y;
            }
            break;

        case NoticeKind::ItemAppeared:
            client->seen[notice.subject] = Sighting{notice.x, notice.y, notice.templateId, true};
            break;

        case NoticeKind::Died:
            if (notice.actor == client->self && notice.subject != client->self)
            {
                ++client->kills;
            }

            // The self case is unreachable, and stays here as the statement
            // of that: see OnDropped.
            client->seen.erase(notice.subject);
            if (notice.subject == client->self)
            {
                client->onMap = false;
            }
            break;

        case NoticeKind::Despawned:
            if (notice.actor == client->self && notice.subject != client->self)
            {
                ++client->pickups;
            }

            client->seen.erase(notice.subject);
            if (notice.subject == client->self)
            {
                client->onMap = false;
            }
            break;

        case NoticeKind::Damaged:
            break;
        }

        if (m_quiet)
        {
            return;
        }

        std::printf("[t=%6.2f] sim=%u conn=%llu  %-9s subj=%u (%d,%d)", Now(), static_cast<unsigned>(simulation),
                    static_cast<unsigned long long>(connection), KindName(notice.kind),
                    static_cast<unsigned>(notice.subject), notice.x, notice.y);

        if (notice.kind == NoticeKind::Moved)
        {
            std::printf(" from (%d,%d)", notice.fromX, notice.fromY);
        }

        if (notice.kind == NoticeKind::Damaged)
        {
            std::printf(" -%d hp=%d", notice.amount, notice.remaining);
        }

        std::printf("\n");
    }

    // The one thing a client cannot learn from a notice: that it is no
    // longer anywhere. A dead player's entity is gone before delivery runs,
    // so it never receives its own death -- see BroadcastSystem.
    void OnDropped(ConnectionId connection, SimulationId simulation)
    {
        ++m_dropCount;

        FakeClient* client = ClientFor(connection);
        if (client == nullptr)
        {
            return;
        }

        // Counted here rather than from a Died notice, because there is no
        // Died notice to count: the player's entity is removed at the
        // barrier, and delivery runs after, so a dead client is gone before
        // anything could tell it so. The drop handler is the only way out
        // here ever learns.
        ++client->deaths;

        client->Forget();
        client->rejoinTimer = kRejoinDelay;

        std::printf("[t=%6.2f] sim=%u conn=%llu  DROPPED -- rejoining in %d ticks\n", Now(),
                    static_cast<unsigned>(simulation), static_cast<unsigned long long>(connection), kRejoinDelay);
    }

    // Every client decides what to do next, from what it knows. Runs
    // between ticks, on the driver thread, which is where a real server
    // would be draining sockets instead.
    void DriveClients()
    {
        for (FakeClient& client : m_clients)
        {
            if (client.rejoinTimer >= 0)
            {
                if (--client.rejoinTimer < 0)
                {
                    // Somewhere new, so a run does not settle into everyone
                    // sitting on the map they started on.
                    client.simulation = static_cast<SimulationId>(m_rng.Range(kSimulationCount) + 1);
                    Join(client);
                }

                continue;
            }

            if (!client.onMap)
            {
                continue;
            }

            if (--client.warpTimer <= 0)
            {
                client.warpTimer = kWarpInterval;

                const SimulationId destination =
                    static_cast<SimulationId>((client.simulation % kSimulationCount) + 1);

                if (m_world.Warp(client.connection, destination, 4 + m_rng.Range(6), 4 + m_rng.Range(6)))
                {
                    ++m_warpCount;
                    std::printf("[t=%6.2f] conn=%llu  WARP sim=%u -> sim=%u\n", Now(),
                                static_cast<unsigned long long>(client.connection),
                                static_cast<unsigned>(client.simulation), static_cast<unsigned>(destination));

                    client.Forget();
                    continue;
                }
            }

            Act(client);
        }
    }

    void Act(FakeClient& client)
    {
        Entity nearestMonster = kNullEntity;
        Entity nearestItem = kNullEntity;
        int monsterDistance = 0;
        int itemDistance = 0;
        int monsterX = 0;
        int monsterY = 0;

        for (const auto& [entity, sighting] : client.seen)
        {
            const int dx = sighting.x - client.x;
            const int dy = sighting.y - client.y;
            const int distance = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);

            if (sighting.item)
            {
                if (nearestItem == kNullEntity || distance < itemDistance)
                {
                    nearestItem = entity;
                    itemDistance = distance;
                }

                continue;
            }

            // Other players are sightings too, and are not to be attacked.
            if (sighting.templateId >= kPlayerTemplateBase)
            {
                continue;
            }

            if (nearestMonster == kNullEntity || distance < monsterDistance)
            {
                nearestMonster = entity;
                monsterDistance = distance;
                monsterX = sighting.x;
                monsterY = sighting.y;
            }
        }

        if (nearestItem != kNullEntity && itemDistance <= 1)
        {
            m_sent += m_world.Send(client.connection, PickupCommand{0, nearestItem}) ? 1 : 0;
            return;
        }

        if (nearestMonster != kNullEntity && monsterDistance <= 1)
        {
            m_sent += m_world.Send(client.connection, AttackCommand{0, nearestMonster, kPlayerDamage}) ? 1 : 0;
            return;
        }

        int directionX = 0;
        int directionY = 0;

        if (nearestMonster != kNullEntity)
        {
            directionX = Sign(monsterX - client.x);
            directionY = Sign(monsterY - client.y);
        }
        else
        {
            directionX = m_rng.Range(3) - 1;
            directionY = m_rng.Range(3) - 1;
        }

        m_sent += m_world.Send(client.connection, StepCommand{0, directionX, directionY}) ? 1 : 0;
    }

    static int Sign(int value)
    {
        return value > 0 ? 1 : (value < 0 ? -1 : 0);
    }

    void PrintSummary()
    {
        std::printf("--- t=%5.1fs  routed=%zu", Now() + kTickSeconds, m_world.ConnectionCount());

        for (std::size_t index = 0; index < m_world.Count(); ++index)
        {
            Simulation& simulation = m_world.At(index);
            std::printf("  | sim%u players=%zu cmds=%zu", static_cast<unsigned>(simulation.Id()),
                        simulation.PlayerCount(), simulation.LastCommandCount());
        }

        std::printf("\n");
    }

    void PrintTotals()
    {
        std::printf("\n=== %d ticks (%.1fs) ===\n", m_totalTicks, static_cast<float>(m_totalTicks) * kTickSeconds);
        std::printf("commands sent   %d\n", m_sent);
        std::printf("notices routed  %lld\n", static_cast<long long>(m_noticeCount));
        std::printf("warps           %d\n", m_warpCount);
        std::printf("drops           %d\n", m_dropCount);

        for (std::size_t index = 0; index < m_world.Count(); ++index)
        {
            Simulation& simulation = m_world.At(index);
            std::printf("sim%u  players=%zu  rejectedJoins=%zu  orphanedCommands=%zu  unhandled=%zu\n",
                        static_cast<unsigned>(simulation.Id()), simulation.PlayerCount(),
                        simulation.RejectedJoinCount(), simulation.OrphanedCommandCount(),
                        simulation.Commands().UnhandledCount());
        }

        for (const FakeClient& client : m_clients)
        {
            std::printf("conn=%llu  sim=%u  kills=%d deaths=%d pickups=%d\n",
                        static_cast<unsigned long long>(client.connection), static_cast<unsigned>(client.simulation),
                        client.kills, client.deaths, client.pickups);
        }
    }

    World m_world;
    std::vector<FakeClient> m_clients;
    Rng m_rng;

    int m_totalTicks = 0;
    bool m_quiet = false;
    int m_tick = 0;

    int m_sent = 0;
    long long m_noticeCount = 0;
    int m_warpCount = 0;
    int m_dropCount = 0;
};

} // namespace

int main(int argc, char** argv)
{
    int seconds = 15;
    bool quiet = false;

    for (int index = 1; index < argc; ++index)
    {
        if (std::strcmp(argv[index], "--quiet") == 0)
        {
            quiet = true;
            continue;
        }

        seconds = std::atoi(argv[index]);
        if (seconds <= 0)
        {
            seconds = 15;
        }
    }

    std::printf("world_v2 runner -- %d simulations, %d clients, %dHz, %ds%s\n\n", kSimulationCount, kClientCount,
                kTicksPerSecond, seconds, quiet ? " (quiet)" : "");

    Runner runner(seconds * kTicksPerSecond, quiet);
    runner.Run();
    return 0;
}
