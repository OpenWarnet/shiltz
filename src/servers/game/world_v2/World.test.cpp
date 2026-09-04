// The routing layer: several simulations under one World, and the seam
// where connections meet entities.
//
// Everything below is about the boundary rather than the simulation. The
// maps here are almost empty on purpose -- Scenario.test.cpp already runs a
// busy world for hundreds of ticks, and repeating that would only make it
// harder to see which of these checks broke.
//
// What each test is for
// ---------------------
//   APacketOnlyReachesItsOwnMap        the containment claim, directly
//   JoinAndLeaveRoundTrip              both halves of routing, both ways
//   ARefusedJoinIsReportedNotLeaked    a spawner that says no
//   APlayerWhoDiesStopsBeingRouted     death without a leave
//   AWarpIsNeverOnTwoMapsAtOnce        the deferred handoff, checked every
//                                      tick across it
//   NoticesReachOnlyWhoCanSeeThem      outbound filtering, and that it does
//                                      not cross a map boundary
//   SendFromManyThreadsWhileTicking    the inbound seam under contention

#include "World.h"

#include "component/Combat.h"
#include "component/Grid.h"
#include "component/Network.h"
#include "core/Entity.h"
#include "core/Test.h"
#include "system/CombatRules.h"
#include "world/MapWorld.h"

#include <atomic>
#include <cstddef>
#include <thread>
#include <utility>
#include <vector>

using namespace world_v2;

namespace
{

constexpr int kMapSize = 16;
constexpr int kViewRadius = 4;

// One 20Hz tick. The number matters only where movement does: at
// GridMovementSystem's default of 4 tiles per second, a step takes five of
// these to finish, which is why the tests that move somebody step once and
// look at the notice rather than waiting for arrival.
constexpr float kStep = 0.05f;

// A client asking to step one tile. The shape World::Send requires: a
// `connection` field and nothing else it has to understand.
struct StepCommand
{
    ConnectionId connection = kInvalidConnection;
    int directionX = 0;
    int directionY = 0;
};

// Kills the sender outright. Stands in for anything that takes a player's
// health to zero -- what matters here is that the routing table notices,
// not what did it.
struct SmiteCommand
{
    ConnectionId connection = kInvalidConnection;
};

// The game layer's half: what a player is made of, and what its commands
// do. Everything world_v2 deliberately does not know.
void InstallPlayerRules(Simulation& simulation, int* stepsSeen = nullptr, bool refuseJoins = false)
{
    InstallCombatRules(simulation.World());

    simulation.OnSpawnPlayer([refuseJoins](MapWorld& world, const JoinCommand& command) -> Entity {
        if (refuseJoins)
        {
            return kNullEntity;
        }

        const Entity player = world.Spawn(command.x, command.y);
        if (player == kNullEntity)
        {
            return kNullEntity;
        }

        world.registry.Assign<ViewerComponent>(player, kViewRadius);
        world.registry.Assign<HealthComponent>(player, 100, 100);
        world.registry.Assign<FactionComponent>(player, 1);
        return player;
    });

    simulation.OnPlayerCommand<StepCommand>([stepsSeen](MapWorld& world, Entity actor, const StepCommand& command) {
        if (stepsSeen != nullptr)
        {
            ++(*stepsSeen);
        }

        world.registry.Assign<MoveIntentComponent>(actor, command.directionX, command.directionY);
    });

    simulation.OnPlayerCommand<SmiteCommand>([](MapWorld& world, Entity actor, const SmiteCommand&) {
        world.registry.Get<HealthComponent>(actor).current = 0;
    });
}

struct DropRecord
{
    ConnectionId connection = kInvalidConnection;
    SimulationId simulation = kInvalidSimulationId;
};

struct NoticeRecord
{
    ConnectionId connection = kInvalidConnection;
    SimulationId simulation = kInvalidSimulationId;
    NoticeKind kind = NoticeKind::Spawned;
};

// --------------------------------------------------------------------

// The claim the whole arrangement rests on: a command from a client on one
// map is not merely ignored by the others, it never reaches them.
void APacketOnlyReachesItsOwnMap()
{
    World world;

    int stepsOnOne = 0;
    int stepsOnTwo = 0;

    Simulation& one = world.Create(1, kMapSize, kMapSize, true);
    Simulation& two = world.Create(2, kMapSize, kMapSize, true);
    InstallPlayerRules(one, &stepsOnOne);
    InstallPlayerRules(two, &stepsOnTwo);

    CHECK(world.Join(1, 100, 1, 4, 4));
    CHECK(world.Join(2, 200, 2, 4, 4));
    world.Tick(kStep);

    CHECK_EQ(world.SimulationFor(1), SimulationId{1});
    CHECK_EQ(world.SimulationFor(2), SimulationId{2});
    CHECK_EQ(one.PlayerCount(), std::size_t{1});
    CHECK_EQ(two.PlayerCount(), std::size_t{1});

    CHECK(world.Send(ConnectionId{1}, StepCommand{0, 1, 0}));
    world.Tick(kStep);

    CHECK_EQ(stepsOnOne, 1);
    CHECK_EQ(stepsOnTwo, 0);

    // Only the map the sender is on moved anybody.
    const Entity onOne = one.EntityFor(1);
    const Entity onTwo = two.EntityFor(2);
    CHECK_EQ(one.World().registry.Get<GridPositionComponent>(onOne).x, 5);
    CHECK_EQ(two.World().registry.Get<GridPositionComponent>(onTwo).x, 4);

    // A connection World has never heard of has nowhere to be routed, and
    // says so rather than picking a map.
    CHECK(!world.Send(ConnectionId{99}, StepCommand{0, 1, 0}));
    world.Tick(kStep);
    CHECK_EQ(stepsOnOne, 1);
    CHECK_EQ(stepsOnTwo, 0);
}

// Both directions of the mapping, and that a deliberate leave is not
// reported as something going wrong.
void JoinAndLeaveRoundTrip()
{
    World world;

    std::vector<DropRecord> drops;
    world.OnConnectionDropped([&drops](ConnectionId connection, SimulationId simulation) {
        drops.push_back(DropRecord{connection, simulation});
    });

    Simulation& only = world.Create(1, kMapSize, kMapSize, true);
    InstallPlayerRules(only);

    CHECK(world.Join(7, 42, 1, 3, 3));
    CHECK(!world.Join(7, 42, 1, 3, 3));  // already somewhere
    CHECK(!world.Join(8, 42, 9, 3, 3));  // no such simulation
    CHECK(!world.Join(kInvalidConnection, 42, 1, 3, 3));
    CHECK_EQ(world.ConnectionCount(), std::size_t{1});

    world.Tick(kStep);

    const Entity player = only.EntityFor(7);
    CHECK(player != kNullEntity);
    CHECK_EQ(only.ConnectionFor(player), ConnectionId{7});
    CHECK_EQ(only.PlayerCount(), std::size_t{1});

    const PlayerSessionComponent* session = only.World().registry.TryGet<PlayerSessionComponent>(player);
    CHECK(session != nullptr);
    if (session != nullptr)
    {
        CHECK_EQ(session->connection, ConnectionId{7});
        CHECK_EQ(session->character, CharacterId{42});
    }

    CHECK(world.Leave(7));
    CHECK(!world.Leave(7));
    CHECK_EQ(world.ConnectionCount(), std::size_t{0});

    world.Tick(kStep);

    CHECK_EQ(only.EntityFor(7), kNullEntity);
    CHECK_EQ(only.PlayerCount(), std::size_t{0});
    CHECK(!only.World().registry.Exists(player));

    // A client that asked to leave was not dropped -- the distinction the
    // drop handler exists to make.
    CHECK_EQ(drops.size(), std::size_t{0});
    CHECK_EQ(only.RejectedJoinCount(), std::size_t{0});
}

// A join the game layer refuses. World cannot see the refusal directly --
// the spawner returns into the simulation thread, not into World -- so it
// is inferred from the client never arriving, and must not leave a session
// waiting forever for somebody who is not coming.
void ARefusedJoinIsReportedNotLeaked()
{
    World world;

    std::vector<DropRecord> drops;
    world.OnConnectionDropped([&drops](ConnectionId connection, SimulationId simulation) {
        drops.push_back(DropRecord{connection, simulation});
    });

    Simulation& only = world.Create(1, kMapSize, kMapSize, true);
    InstallPlayerRules(only, nullptr, /*refuseJoins=*/true);

    CHECK(world.Join(5, 1, 1, 3, 3));
    CHECK_EQ(world.ConnectionCount(), std::size_t{1});

    world.Tick(kStep);
    CHECK_EQ(only.RejectedJoinCount(), std::size_t{1});

    // Still held, one tick of grace left: giving up here would report a
    // client as dropped the same tick a slower join would have landed.
    CHECK_EQ(world.ConnectionCount(), std::size_t{1});
    CHECK_EQ(drops.size(), std::size_t{0});

    world.Tick(kStep);

    CHECK_EQ(drops.size(), std::size_t{1});
    if (!drops.empty())
    {
        CHECK_EQ(drops[0].connection, ConnectionId{5});
        CHECK_EQ(drops[0].simulation, SimulationId{1});
    }

    CHECK_EQ(world.ConnectionCount(), std::size_t{0});
    CHECK_EQ(world.SimulationFor(5), kInvalidSimulationId);

    // And it stays gone rather than being reported again every tick.
    world.Tick(kStep);
    world.Tick(kStep);
    CHECK_EQ(drops.size(), std::size_t{1});
}

// The case nothing pushes a command for: the player's entity goes away
// underneath the routing table.
void APlayerWhoDiesStopsBeingRouted()
{
    World world;

    std::vector<DropRecord> drops;
    world.OnConnectionDropped([&drops](ConnectionId connection, SimulationId simulation) {
        drops.push_back(DropRecord{connection, simulation});
    });

    int steps = 0;
    Simulation& only = world.Create(1, kMapSize, kMapSize, true);
    InstallPlayerRules(only, &steps);

    CHECK(world.Join(7, 42, 1, 3, 3));
    world.Tick(kStep);
    CHECK(only.EntityFor(7) != kNullEntity);

    // Drains at stage 1, kills at stage 2, is swept away at the barrier,
    // and is unrouted immediately after -- all in the tick below.
    CHECK(world.Send(ConnectionId{7}, SmiteCommand{}));
    world.Tick(kStep);

    CHECK_EQ(drops.size(), std::size_t{1});
    if (!drops.empty())
    {
        CHECK_EQ(drops[0].connection, ConnectionId{7});
        CHECK_EQ(drops[0].simulation, SimulationId{1});
    }

    CHECK_EQ(only.EntityFor(7), kNullEntity);
    CHECK_EQ(only.PlayerCount(), std::size_t{0});
    CHECK_EQ(world.ConnectionCount(), std::size_t{0});
    CHECK_EQ(world.SimulationFor(7), kInvalidSimulationId);

    // The next packet from that client has nowhere to go, and is refused
    // out here rather than landing on whoever inherited the slot.
    CHECK(!world.Send(ConnectionId{7}, StepCommand{0, 1, 0}));
    world.Tick(kStep);
    CHECK_EQ(steps, 0);
}

// The reason a warp costs two ticks instead of one.
void AWarpIsNeverOnTwoMapsAtOnce()
{
    World world;

    std::vector<DropRecord> drops;
    world.OnConnectionDropped([&drops](ConnectionId connection, SimulationId simulation) {
        drops.push_back(DropRecord{connection, simulation});
    });

    Simulation& one = world.Create(1, kMapSize, kMapSize, true);
    Simulation& two = world.Create(2, kMapSize, kMapSize, true);
    InstallPlayerRules(one);
    InstallPlayerRules(two);

    CHECK(world.Join(7, 42, 1, 3, 3));

    // Not landed yet: warping now would push a leave that arrives before
    // the join it is meant to undo.
    CHECK(!world.Warp(7, 2, 10, 10));

    world.Tick(kStep);
    CHECK(one.EntityFor(7) != kNullEntity);

    CHECK(!world.Warp(7, 1, 10, 10));  // already there
    CHECK(!world.Warp(7, 9, 10, 10));  // no such simulation
    CHECK(!world.Warp(99, 2, 10, 10)); // no such connection

    CHECK(world.Warp(7, 2, 10, 10));
    CHECK(!world.Warp(7, 2, 10, 10));  // one at a time

    // In flight, the client is between maps and has nowhere to send to.
    CHECK(!world.Send(ConnectionId{7}, StepCommand{0, 1, 0}));

    // The invariant, checked on every tick across the handoff rather than
    // only at the end -- a duplicate that exists for one tick and then
    // resolves is exactly the bug this design is avoiding.
    for (int tick = 0; tick < 6; ++tick)
    {
        world.Tick(kStep);
        const bool onOne = one.EntityFor(7) != kNullEntity;
        const bool onTwo = two.EntityFor(7) != kNullEntity;
        CHECK(!(onOne && onTwo));
    }

    CHECK_EQ(world.SimulationFor(7), SimulationId{2});
    CHECK_EQ(one.PlayerCount(), std::size_t{0});
    CHECK_EQ(two.PlayerCount(), std::size_t{1});

    const Entity arrived = two.EntityFor(7);
    CHECK(arrived != kNullEntity);
    if (arrived != kNullEntity)
    {
        const GridPositionComponent& position = two.World().registry.Get<GridPositionComponent>(arrived);
        CHECK_EQ(position.x, 10);
        CHECK_EQ(position.y, 10);
        CHECK_EQ(two.ConnectionFor(arrived), ConnectionId{7});
    }

    // Leaving one map on the way to another is not a disconnection.
    CHECK_EQ(drops.size(), std::size_t{0});

    // And the client is routable again on the far side.
    CHECK(world.Send(ConnectionId{7}, StepCommand{0, 1, 0}));
}

// Outbound, in both senses: a notice reaches the connections in range and
// no others, and it does not leave the map it happened on.
void NoticesReachOnlyWhoCanSeeThem()
{
    World world;

    std::vector<NoticeRecord> notices;
    world.OnNotice([&notices](ConnectionId connection, SimulationId simulation, const Notice& notice) {
        notices.push_back(NoticeRecord{connection, simulation, notice.kind});
    });

    Simulation& one = world.Create(1, kMapSize, kMapSize, true);
    Simulation& two = world.Create(2, kMapSize, kMapSize, true);
    InstallPlayerRules(one);
    InstallPlayerRules(two);

    // Mover and a near witness on map 1, a far one out of range, and a
    // fourth client standing on the *same coordinates* on map 2 -- which is
    // what makes this a test of the map boundary rather than of distance.
    CHECK(world.Join(1, 101, 1, 4, 4));
    CHECK(world.Join(2, 102, 1, 6, 4));
    CHECK(world.Join(3, 103, 1, 14, 14));
    CHECK(world.Join(4, 104, 2, 4, 4));

    world.Tick(kStep);
    notices.clear();

    CHECK(world.Send(ConnectionId{1}, StepCommand{0, 1, 0}));
    world.Tick(kStep);

    int moverHeard = 0;
    int neighbourHeard = 0;
    int distantHeard = 0;
    int otherMapHeard = 0;

    for (const NoticeRecord& record : notices)
    {
        CHECK(record.kind == NoticeKind::Moved);

        if (record.connection == 1)
        {
            CHECK_EQ(record.simulation, SimulationId{1});
            ++moverHeard;
        }
        else if (record.connection == 2)
        {
            CHECK_EQ(record.simulation, SimulationId{1});
            ++neighbourHeard;
        }
        else if (record.connection == 3)
        {
            ++distantHeard;
        }
        else if (record.connection == 4)
        {
            ++otherMapHeard;
        }
    }

    CHECK_EQ(moverHeard, 1);
    CHECK_EQ(neighbourHeard, 1);
    CHECK_EQ(distantHeard, 0);

    // The one that would be a routing bug rather than a filtering one: a
    // client on another map, at coordinates that would have passed the
    // range test, hearing about something that did not happen where it is.
    CHECK_EQ(otherMapHeard, 0);
}

// The inbound seam as it will actually be used: many connection threads
// pushing while the driver thread ticks.
//
// The point is not throughput. It is that nothing is lost and nothing is
// misrouted -- every command that World accepted arrives at the one
// simulation its sender was on.
void SendFromManyThreadsWhileTicking()
{
    constexpr int kMaps = 4;
    constexpr int kConnectionsPerMap = 4;
    constexpr int kSenders = 4;
    // A whole multiple of the connection count, so the round robin below
    // hits every client the same number of times whatever offset a sender
    // starts at. That is what makes the per-map split at the end an exact
    // number rather than an approximate one.
    constexpr int kSendsPerSender = 256;

    World world;

    int steps[kMaps] = {0, 0, 0, 0};
    for (int map = 0; map < kMaps; ++map)
    {
        Simulation& simulation = world.Create(static_cast<SimulationId>(map + 1), kMapSize, kMapSize, true);
        InstallPlayerRules(simulation, &steps[map]);
    }

    std::vector<ConnectionId> connections;
    for (int map = 0; map < kMaps; ++map)
    {
        for (int slot = 0; slot < kConnectionsPerMap; ++slot)
        {
            const ConnectionId connection = static_cast<ConnectionId>(map * 100 + slot + 1);
            CHECK(world.Join(connection, 1, static_cast<SimulationId>(map + 1), 2 + slot, 2));
            connections.push_back(connection);
        }
    }

    // Everybody on the map before the senders start, so an early refusal
    // cannot be mistaken for a lost command.
    world.Tick(kStep);
    world.Tick(kStep);
    CHECK_EQ(world.ConnectionCount(), connections.size());

    std::atomic<int> accepted{0};
    std::atomic<bool> sending{true};

    std::vector<std::thread> senders;
    senders.reserve(kSenders);

    for (int sender = 0; sender < kSenders; ++sender)
    {
        senders.emplace_back([&world, &connections, &accepted, sender]() {
            for (int index = 0; index < kSendsPerSender; ++index)
            {
                const ConnectionId connection = connections[(sender + index) % connections.size()];

                // Zero direction on purpose: the handler counts it, and
                // GridMovementSystem consumes the intent without moving
                // anybody, so the run does not turn into a movement test.
                if (world.Send(connection, StepCommand{0, 0, 0}))
                {
                    accepted.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // Ticking the whole time, so pushes land against a world that is
    // actively draining rather than a parked one.
    while (sending.load(std::memory_order_relaxed))
    {
        world.Tick(kStep);

        // Every send is accepted here -- nobody leaves, dies, or warps --
        // so the count reaching its ceiling is the senders being finished.
        if (accepted.load(std::memory_order_relaxed) >= kSenders * kSendsPerSender)
        {
            sending.store(false, std::memory_order_relaxed);
        }
    }

    for (std::thread& sender : senders)
    {
        sender.join();
    }

    // Anything pushed after the last tick still has to drain.
    world.Tick(kStep);
    world.Tick(kStep);

    CHECK_EQ(accepted.load(), kSenders * kSendsPerSender);

    int handled = 0;
    for (int map = 0; map < kMaps; ++map)
    {
        handled += steps[map];

        Simulation& simulation = world.At(static_cast<std::size_t>(map));
        CHECK_EQ(simulation.Commands().PendingCount(), std::size_t{0});
        CHECK_EQ(simulation.Commands().UnhandledCount(), std::size_t{0});

        // Nobody left, died, or warped, so no command should have arrived
        // to find its sender gone.
        CHECK_EQ(simulation.OrphanedCommandCount(), std::size_t{0});
        CHECK_EQ(simulation.PlayerCount(), std::size_t{kConnectionsPerMap});
    }

    CHECK_EQ(handled, accepted.load());

    // Each map handled only what its own clients sent. With the round robin
    // above that is an even split, and an uneven one would mean a command
    // crossed a boundary.
    for (int map = 0; map < kMaps; ++map)
    {
        CHECK_EQ(steps[map], (kSenders * kSendsPerSender) / kMaps);
    }
}

} // namespace

int main()
{
    APacketOnlyReachesItsOwnMap();
    JoinAndLeaveRoundTrip();
    ARefusedJoinIsReportedNotLeaked();
    APlayerWhoDiesStopsBeingRouted();
    AWarpIsNeverOnTwoMapsAtOnce();
    NoticesReachOnlyWhoCanSeeThem();
    SendFromManyThreadsWhileTicking();

    return world_v2::test::Summary("world");
}
