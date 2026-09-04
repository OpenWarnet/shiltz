#pragma once

#include "CommandQueue.h"
#include "Ids.h"
#include "component/Network.h"
#include "system/AISystem.h"
#include "system/BroadcastSystem.h"
#include "system/CombatSystem.h"
#include "system/DeathSystem.h"
#include "system/DespawnSystem.h"
#include "system/GridMovementSystem.h"
#include "system/PickupSystem.h"
#include "system/SpawnSystem.h"
#include "world/MapWorld.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace world_v2
{

// A client asking to be placed on this map.
//
// A command rather than a method for the same reason every other inbound
// thing is one: a join creates an entity, which is structural, and
// structural change may only happen at a point in the tick where nothing is
// iterating. A socket thread that called a Join() method directly would be
// reaching into the Registry mid-sweep -- exactly what CommandQueue exists
// to prevent.
//
// Carries an id, not a character. Loading a character's level, stats, and
// inventory is I/O, and doing I/O on the simulation thread would stall
// every other player on the map for the duration. The layer above is
// expected to have the character in hand already and to capture whatever it
// needs in the spawn callback, keyed by this id.
//
// x and y are where the client should appear -- a warp destination, a save
// point, a town square. Refusing an unwalkable or out-of-bounds spot is the
// spawn callback's decision, not this struct's.
struct JoinCommand
{
    ConnectionId connection = kInvalidConnection;
    CharacterId character = kInvalidCharacter;
    int x = 0;
    int y = 0;
};

// A client leaving this map -- logging out, or the first half of a warp.
//
// Idempotent by design: leaving a map the connection is not on does
// nothing. A disconnect racing a warp is normal, and both arriving is not
// worth an error path.
struct LeaveCommand
{
    ConnectionId connection = kInvalidConnection;
};

// Builds the entity a joining client will play, and returns it --
// kNullEntity to refuse the join.
//
// This is where everything world_v2 deliberately does not know lives:
// which components a player has, what their stats are, how wide they see,
// whether that tile is a legal place to stand. Runs on the simulation
// thread, at the top of the tick, so it must not do I/O -- see JoinCommand.
//
// Simulation attaches the PlayerSessionComponent itself afterwards, so a
// callback that forgets to cannot break routing.
using PlayerSpawner = std::function<Entity(MapWorld&, const JoinCommand&)>;

// A command that came from a particular client.
//
// The connection is the only identity a socket thread can safely put in a
// command. It cannot look up the sender's Entity itself: the table that
// answers that question is owned by the simulation thread and is being
// rewritten by joins, leaves, and deaths while the packet is in flight. So
// the command carries the durable id, and the resolution happens where the
// answer is stable -- see OnPlayerCommand.
template <typename T>
concept PlayerCommand = requires(const T& command) {
    { command.connection } -> std::convertible_to<ConnectionId>;
};

// Takes a leaving client's entity out of the world. Defaults to a plain
// MapWorld::Despawn; override it when leaving has to save state, drop a
// corpse, or hand carried items somewhere first.
using PlayerDespawner = std::function<void(MapWorld&, Entity)>;

// One map's tick.
//
// The stage order is not a style choice -- it is what makes the rest of the
// framework safe to write against. Systems sweep packed arrays that move
// when anything is added or removed, so the tick is arranged to give
// structural change exactly one place to happen, and to keep every system
// out of it.
//
//   1 INBOUND        Drain the command queue. Everything other threads
//                    asked for this tick becomes state here, before any
//                    system starts iterating -- so a system's view of the
//                    world does not change under it partway through.
//
//   2 SIMULATION     The systems, in dependency order. They read and write
//                    component *values* freely. The only structural change
//                    any of them may make is to the entity its own view is
//                    currently visiting (View.h says why that one is safe).
//                    Anything wider -- spawning, despawning, giving an
//                    unrelated entity a component -- is emitted as an event
//                    instead.
//
//   3 BARRIER        events.Flush(). The one place structural change is
//                    unrestricted, because nothing is iterating. Deaths
//                    resolve, loot spawns, level-ups apply, and cascades
//                    settle in the same flush.
//
//   4 OUTBOUND       Read the settled state and build whatever goes out on
//                    the wire. Nothing here modifies anything; by this
//                    point the tick's story is finished and the job is to
//                    describe it.
//
// Sessions
// --------
// A Simulation also keeps the connection -> Entity half of routing for the
// clients on this map, so that whoever is holding sockets can turn a packet
// into a command without a table of its own. It is a private map touched
// only by the join and leave handlers, which run at stage 1 -- so it is as
// single-threaded as everything else here, and needs no lock even when
// several simulations tick at once.
//
// A player entity can also stop existing without a LeaveCommand: it died,
// or something despawned it. The routing entry is reaped right after the
// barrier and reported through DroppedConnections(), because a stale entry
// is worse than a missing one -- it would route the next packet at whoever
// inherited the slot.
//
// Threading
// ---------
// Everything above runs on one thread. Other threads reach a Simulation
// only by pushing to Commands(), which is the sole synchronized surface.
// Two Ticks must never overlap -- a debug assertion catches that directly
// rather than letting it turn into a rare corrupt-pool crash.
//
// One Simulation per map. Maps share no state, so several can tick on
// separate threads at once, provided no single one is ticked twice at once.
class Simulation
{
public:
    // `id` is trailing and defaulted because most simulations have no need
    // of one: a standalone simulation -- every test in this directory, the
    // benchmark, the ones a World never sees -- is not a key in anybody's
    // table, and kInvalidSimulationId says so honestly.
    Simulation(int width, int height, bool walkableByDefault = false, SimulationId id = kInvalidSimulationId)
        : m_id(id)
        , m_world(width, height, walkableByDefault)
    {
        // Registered here rather than left to the caller so that a notice
        // can never be missed because collection was wired up after the
        // first tick.
        m_broadcast.Install(m_world);

        // Reserved: JoinCommand and LeaveCommand are the two command types
        // Simulation handles itself, and registering either from outside
        // would replace this and quietly break routing. Anything a game
        // layer wants to happen on join belongs in the PlayerSpawner.
        m_commands.On<JoinCommand>([this](MapWorld& world, const JoinCommand& command) {
            ApplyJoin(world, command);
        });

        m_commands.On<LeaveCommand>([this](MapWorld& world, const LeaveCommand& command) {
            ApplyLeave(world, command);
        });
    }

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    SimulationId Id() const
    {
        return m_id;
    }

    MapWorld& World()
    {
        return m_world;
    }

    // The inbound seam. Safe to touch from any thread -- see CommandQueue.
    CommandQueue& Commands()
    {
        return m_commands;
    }

    // Setup only, before the first tick. Without a spawner every join is
    // refused, which is the right failure: an unconfigured simulation
    // silently accepting players and giving them no body would be worse.
    void OnSpawnPlayer(PlayerSpawner spawner)
    {
        m_spawnPlayer = std::move(spawner);
    }

    void OnDespawnPlayer(PlayerDespawner despawner)
    {
        m_despawnPlayer = std::move(despawner);
    }

    // Registers a handler for a command sent by a client, with the sender
    // already resolved to a live Entity.
    //
    // This is the registration a game layer should reach for. Handling the
    // same command through CommandQueue::On directly means receiving a
    // ConnectionId and doing this resolution by hand, and CommandQueue's
    // header explains what happens the one time somebody forgets: the
    // player disconnected between the push and the drain, the handle is
    // stale, and the write lands on whoever inherited the slot.
    //
    // A command whose sender is no longer on this map -- left, warped,
    // died, or never joined -- is dropped and counted, not delivered. That
    // is a normal race, not an error: the packet was in flight while the
    // world moved on.
    //
    // Setup only, before the first tick, like every other On.
    template <PlayerCommand T>
    void OnPlayerCommand(std::function<void(MapWorld&, Entity, const T&)> handler)
    {
        m_commands.On<T>([this, handler = std::move(handler)](MapWorld& world, const T& command) {
            const Entity actor = EntityFor(command.connection);
            if (actor == kNullEntity)
            {
                ++m_orphanedCommands;
                return;
            }

            handler(world, actor, command);
        });
    }

    // The entity `connection` is playing here, or kNullEntity if it is not
    // on this map -- or if its entity stopped existing since the last
    // reap, which is checked rather than assumed.
    //
    // Simulation thread only. Everyone else asks by pushing a command.
    Entity EntityFor(ConnectionId connection) const
    {
        const auto it = m_connections.find(connection);
        if (it == m_connections.end())
        {
            return kNullEntity;
        }

        return m_world.registry.Exists(it->second) ? it->second : kNullEntity;
    }

    // The other direction, straight off the entity. kInvalidConnection for
    // a monster, an item, or anything else nobody is playing.
    ConnectionId ConnectionFor(Entity entity)
    {
        const PlayerSessionComponent* session = m_world.registry.TryGet<PlayerSessionComponent>(entity);
        return session ? session->connection : kInvalidConnection;
    }

    // How many clients this map is routing for.
    std::size_t PlayerCount() const
    {
        return m_connections.size();
    }

    // Connections whose entity stopped existing during the last tick
    // without a LeaveCommand -- a death, or a despawn from anywhere else.
    //
    // Valid from the moment Tick returns until the next Tick clears it. The
    // layer above is expected to drain it every tick; a connection listed
    // here is no longer on this map and its next packet has nowhere to go.
    const std::vector<ConnectionId>& DroppedConnections() const
    {
        return m_dropped;
    }

    // Player commands dropped because their sender was no longer on this
    // map by the time they drained. A steady trickle is normal; a large
    // and growing number means clients are being routed to a map they have
    // already left.
    std::size_t OrphanedCommandCount() const
    {
        return m_orphanedCommands;
    }

    // Joins refused since construction: no spawner installed, a duplicate
    // connection, or a spawner that returned kNullEntity. A counter rather
    // than a log because the interesting number is whether it is zero.
    std::size_t RejectedJoinCount() const
    {
        return m_rejectedJoins;
    }

    // Fills every spawner to strength and resolves it immediately, for
    // world load. Setup only -- call it after the spawners and the spawn
    // rules are in place, and before the first Tick.
    void PrimeSpawns()
    {
        m_spawn.Prime(m_world.registry, m_world.events);
        m_world.events.Flush();
    }

    // Stage 4. Called once per (viewer, notice) pair that the viewer can
    // see, after the world has settled. Set once during setup.
    void OnNotice(NoticeSink sink)
    {
        m_sink = std::move(sink);
    }

    BroadcastSystem& Broadcast()
    {
        return m_broadcast;
    }

    // Stage 4, raw. Called at the end of every tick with the settled world,
    // after notices have been delivered. For anything the notice stream
    // does not cover.
    //
    // Takes a non-const MapWorld& only because building a broadcast needs
    // views, and a view has to materialize pools. The contract is still
    // read-only: this runs after the barrier, and a structural change made
    // here would land in a tick that has already finished reasoning about
    // itself.
    void OnBroadcast(std::function<void(MapWorld&)> callback)
    {
        m_rawBroadcast = std::move(callback);
    }

    // How many commands the last tick drained. Cheap visibility into
    // whether inbound work is actually arriving.
    std::size_t LastCommandCount() const
    {
        return m_lastCommandCount;
    }

    void Tick(float deltaSeconds)
    {
        // Not a lock -- a lock would make overlapping ticks *work*, quietly,
        // at a cost the design has already refused to pay. This asserts the
        // contract instead, so a caller that ticks the same map from two
        // threads finds out immediately in a debug build, rather than as a
        // corrupted pool weeks later.
        assert(!m_ticking && "world_v2: Simulation::Tick re-entered or ticked concurrently");
        m_ticking = true;

        // --- 1. Inbound -----------------------------------------------
        m_lastCommandCount = m_commands.Drain(m_world);

        // --- 2. Simulation --------------------------------------------
        // The order is a dependency chain, not a preference. AI writes the
        // intents and attack requests; movement resolves the intents, which
        // is what decides who is standing where; combat then checks reach
        // against those settled positions; death notices what combat left
        // at zero. Running any of them earlier would have it act on last
        // tick's answer.
        //
        // Spawning leads because it only counts and emits -- the monsters
        // it asks for are created at the barrier, so they first draw breath
        // at the end of this tick and first act in the next one.
        //
        // Pickup sits after movement for the same reason combat does: reach
        // has to be judged against where things actually ended up, not
        // where they were when the packet arrived.
        m_spawn.Update(m_world.registry, m_world.events, deltaSeconds);
        m_ai.Update(m_world.registry, m_world.tiles);
        m_movement.Update(m_world.registry, m_world.tiles, m_world.events, deltaSeconds);
        m_pickup.Update(m_world.registry, m_world.events);
        m_combat.Update(m_world.registry, m_world.events);
        m_death.Update(m_world.registry, m_world.events);

        // Last of the stage, and after death on purpose. Both end in a
        // despawn at the barrier, and something that dies on the same tick
        // its timer runs out should be reported as having died -- the
        // death handler is the one that credits a killer and drops loot,
        // and an expiry that got there first would announce the removal
        // with none of that.
        m_despawn.Update(m_world.registry, m_world.events, deltaSeconds);

        // --- 3. Barrier -----------------------------------------------
        m_world.events.Flush();

        // Straight after, and before anything reads the settled world: a
        // player who died this tick is gone as of the flush above, and the
        // routing table must not still be claiming otherwise when the
        // outbound stage starts asking who is who.
        ReapDroppedPlayers();

        // --- 4. Outbound ----------------------------------------------
        // Nothing here modifies anything: the tick's story is finished, and
        // the job is to describe it. Deliver also clears the notice list,
        // with or without a sink, so an unwatched simulation does not
        // accumulate one.
        m_broadcast.Deliver(m_world.registry, m_sink);

        if (m_rawBroadcast)
        {
            m_rawBroadcast(m_world);
        }

        m_ticking = false;
    }

private:
    // Stage 1, from the command queue. Everything structural about a join
    // happens here, on the simulation thread, before any system runs.
    void ApplyJoin(MapWorld& world, const JoinCommand& command)
    {
        if (command.connection == kInvalidConnection || !m_spawnPlayer)
        {
            ++m_rejectedJoins;
            return;
        }

        // Already here. Not an error worth crashing over -- a client that
        // sends two joins, or a warp that raced a reconnect, gets the
        // second one ignored rather than a second body.
        if (EntityFor(command.connection) != kNullEntity)
        {
            ++m_rejectedJoins;
            return;
        }

        const Entity entity = m_spawnPlayer(world, command);
        if (entity == kNullEntity || !world.registry.Exists(entity))
        {
            // A refusal, not a failure: the spawner is what knows whether
            // the destination tile is legal.
            ++m_rejectedJoins;
            return;
        }

        // Attached here rather than left to the spawner so the two halves
        // of routing are written in one place and cannot disagree.
        world.registry.Assign<PlayerSessionComponent>(entity, command.connection, command.character);

        // Overwrites any entry left by a connection id that was reused --
        // which Ids.h asks callers not to do, but which would otherwise
        // strand the old entity as unroutable rather than merely wrong.
        m_connections[command.connection] = entity;
    }

    void ApplyLeave(MapWorld& world, const LeaveCommand& command)
    {
        const auto it = m_connections.find(command.connection);
        if (it == m_connections.end())
        {
            return;
        }

        const Entity entity = it->second;
        m_connections.erase(it);

        // The entity may already be gone -- died this tick, despawned last
        // one -- in which case the reap has done, or will do, the rest.
        if (!world.registry.Exists(entity))
        {
            return;
        }

        if (m_despawnPlayer)
        {
            m_despawnPlayer(world, entity);
            return;
        }

        world.Despawn(entity);
    }

    // O(players on this map) per tick, walked rather than maintained.
    //
    // The alternative is for every path that can destroy an entity to
    // remember to check whether it was a player and unroute it -- death,
    // despawn timers, and whatever gets added next. One sweep over a map's
    // clients cannot be forgotten by a system that has not been written
    // yet, and at map-sized player counts it does not show up next to the
    // component sweeps above it.
    void ReapDroppedPlayers()
    {
        m_dropped.clear();

        for (auto it = m_connections.begin(); it != m_connections.end();)
        {
            if (m_world.registry.Exists(it->second))
            {
                ++it;
                continue;
            }

            m_dropped.push_back(it->first);
            it = m_connections.erase(it);
        }
    }

    SimulationId m_id = kInvalidSimulationId;

    MapWorld m_world;
    CommandQueue m_commands;

    SpawnSystem m_spawn;
    AISystem m_ai;
    GridMovementSystem m_movement;
    PickupSystem m_pickup;
    CombatSystem m_combat;
    DeathSystem m_death;
    DespawnSystem m_despawn;

    BroadcastSystem m_broadcast;
    NoticeSink m_sink;
    std::function<void(MapWorld&)> m_rawBroadcast;

    PlayerSpawner m_spawnPlayer;
    PlayerDespawner m_despawnPlayer;

    // connection -> Entity. The other direction is PlayerSessionComponent.
    std::unordered_map<ConnectionId, Entity> m_connections;
    std::vector<ConnectionId> m_dropped;

    std::size_t m_rejectedJoins = 0;
    std::size_t m_orphanedCommands = 0;
    std::size_t m_lastCommandCount = 0;
    bool m_ticking = false;
};

} // namespace world_v2
