#pragma once

#include "Ids.h"
#include "Simulation.h"
#include "system/BroadcastSystem.h"

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace world_v2
{

// Every simulation on this server, and the routing that decides which one a
// client belongs to.
//
// The layer above -- a socket server -- knows about connections and packets
// and nothing about maps. A Simulation knows about entities and tiles and
// nothing about connections. World is the seam: it is the only thing that
// holds both halves, and it is deliberately the only thing that does.
//
//     socket thread            World                    simulation thread
//     -------------            -----                    -----------------
//     packet for conn 7  -->   which simulation?   -->   CommandQueue push
//                                                        (drained next tick)
//
//     serialize + send   <--   which connection?   <--   Notice for viewer E
//
// What World does not do
// ----------------------
// It does not parse packets, own sockets, or know one opcode from another.
// Send<T> takes a command struct the game layer defined and a connection id
// the game layer chose; everything about what that command means happens on
// either side of World, never inside it.
//
// It also does not tick anything on its own. Tick(dt) is called by whoever
// owns the clock -- a server loop, or a test that wants to step 300 times
// as fast as it can.
//
// Threading
// ---------
//   Send, Join, Leave, Warp, SimulationFor   any thread
//   Create, OnNotice, OnConnectionDropped    setup only, before ticking
//   Tick                                     the driver thread, never
//                                            re-entered
//
// The session table is guarded by a shared_mutex because the two sides have
// very different shapes: Send does a lookup on every inbound packet from
// every connection at once, while Join, Leave, and Warp happen a handful of
// times per player per session. Readers that never block each other is
// exactly the case that is for.
//
// Simulations themselves are not guarded, and do not need to be: nothing
// here reaches into one except through its CommandQueue, which is
// synchronized, and between ticks from the driver thread, where nothing
// else is running.
class World
{
public:
    // Stage 4 of some simulation's tick, translated. Called with the
    // connection that should be told, which simulation it happened on, and
    // what happened.
    //
    // Runs on the thread that ticked that simulation. Today Tick walks the
    // simulations in order on one thread, so that is the driver thread; if
    // they are ever ticked in parallel this will be called from several at
    // once, so whatever it writes to had better expect that.
    using NoticeRouter = std::function<void(ConnectionId, SimulationId, const Notice& notice)>;

    // A connection that is no longer on any simulation, without having
    // asked to leave: its character died, something despawned it, or its
    // join was refused. Called at the end of Tick, on the driver thread,
    // holding no lock -- so it is free to call Join to put the player
    // somewhere else.
    using DropHandler = std::function<void(ConnectionId, SimulationId)>;

    World() = default;

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Adds a simulation and returns it, so the caller can install its spawn
    // rules, terrain, and command handlers.
    //
    // Setup only. Simulations are never removed, which is what lets Tick
    // walk them without a lock. `id` must be unique and not
    // kInvalidSimulationId.
    Simulation& Create(SimulationId id, int width, int height, bool walkableByDefault = false)
    {
        assert(!m_ticking && "world_v2: World::Create during a tick");
        assert(id != kInvalidSimulationId && "world_v2: kInvalidSimulationId is not a usable id");
        assert(m_index.find(id) == m_index.end() && "world_v2: duplicate SimulationId");

        m_simulations.push_back(std::make_unique<Simulation>(width, height, walkableByDefault, id));
        Simulation* simulation = m_simulations.back().get();
        m_index.emplace(id, simulation);

        // Installed here rather than left to the caller, for the same
        // reason Simulation installs its own broadcast listeners: a
        // simulation created and then forgotten about would tick perfectly
        // and tell nobody anything.
        //
        // Captures the pointer, not an index -- unique_ptr keeps the
        // Simulation itself still even as the vector grows.
        simulation->OnNotice([this, simulation](Entity viewer, const Notice& notice) {
            const ConnectionId connection = simulation->ConnectionFor(viewer);
            if (connection == kInvalidConnection)
            {
                // A viewer nobody is playing -- an AI's awareness, or a
                // camera. Visible to the simulation, not to the wire.
                return;
            }

            if (m_router)
            {
                m_router(connection, simulation->Id(), notice);
            }
        });

        return *simulation;
    }

    Simulation* Find(SimulationId id)
    {
        const auto it = m_index.find(id);
        return it == m_index.end() ? nullptr : it->second;
    }

    // Creation order, which is also tick order -- so a test that wants two
    // runs to agree exactly gets that for free.
    std::size_t Count() const
    {
        return m_simulations.size();
    }

    Simulation& At(std::size_t index)
    {
        assert(index < m_simulations.size());
        return *m_simulations[index];
    }

    void OnNotice(NoticeRouter router)
    {
        assert(!m_ticking && "world_v2: World::OnNotice during a tick");
        m_router = std::move(router);
    }

    void OnConnectionDropped(DropHandler handler)
    {
        assert(!m_ticking && "world_v2: World::OnConnectionDropped during a tick");
        m_dropped = std::move(handler);
    }

    // --- routing ------------------------------------------------------

    // Puts a client on a simulation at (x, y). False if the connection is
    // already on one, or that simulation does not exist.
    //
    // True means the join was *sent*, not that it succeeded: the spawn
    // callback on the other side still gets to refuse it, and a refusal
    // comes back through the drop handler a tick or two later. Until the
    // join drains, Send for this connection returns true and the command is
    // dropped on arrival -- the client exists to World before it exists to
    // the simulation, and there is no way around that gap short of blocking
    // a socket thread on a tick.
    bool Join(ConnectionId connection, CharacterId character, SimulationId simulation, int x, int y)
    {
        if (connection == kInvalidConnection)
        {
            return false;
        }

        const std::unique_lock<std::shared_mutex> lock(m_mutex);

        if (m_sessions.find(connection) != m_sessions.end())
        {
            return false;
        }

        Simulation* target = FindLocked(simulation);
        if (target == nullptr)
        {
            return false;
        }

        Session session;
        session.simulation = simulation;
        session.character = character;
        m_sessions.emplace(connection, session);

        // Pushed under the lock so that two calls about the same connection
        // reach the queue in the order they were decided here. Safe:
        // CommandQueue takes its own lock and never calls back into World.
        target->Commands().Push(JoinCommand{connection, character, x, y});
        return true;
    }

    // Takes a client off whatever simulation it is on. False if it was not
    // on one. Cancels a warp in flight.
    bool Leave(ConnectionId connection)
    {
        const std::unique_lock<std::shared_mutex> lock(m_mutex);

        const auto it = m_sessions.find(connection);
        if (it == m_sessions.end())
        {
            return false;
        }

        Simulation* source = FindLocked(it->second.simulation);
        m_sessions.erase(it);

        if (source != nullptr)
        {
            source->Commands().Push(LeaveCommand{connection});
        }

        return true;
    }

    // Moves a client from the simulation it is on to another one, landing
    // at (x, y).
    //
    // Two ticks, not one, and deliberately: the leave is pushed now, and
    // the join only goes out once the client is confirmed gone from the old
    // simulation. Pushing both at once would be a tick faster and would
    // mean that whenever the destination happens to tick before the source,
    // the player exists twice -- two bodies, two sets of broadcasts, and a
    // bug that only appears when the map order changes.
    //
    // False if the connection is not on a simulation, is already warping,
    // has not finished arriving where it is, or the destination does not
    // exist or is where it already is.
    bool Warp(ConnectionId connection, SimulationId destination, int x, int y)
    {
        const std::unique_lock<std::shared_mutex> lock(m_mutex);

        const auto it = m_sessions.find(connection);
        if (it == m_sessions.end())
        {
            return false;
        }

        Session& session = it->second;

        // Warping out of a map the client has not landed on yet would mean
        // pushing a leave that arrives before the join it is meant to
        // undo -- after which the join spawns them on a map World has
        // already written off. Refused instead; the caller can try again
        // next tick.
        if (!session.arrived || session.warpTarget != kInvalidSimulationId)
        {
            return false;
        }

        if (destination == session.simulation || FindLocked(destination) == nullptr)
        {
            return false;
        }

        Simulation* source = FindLocked(session.simulation);
        if (source == nullptr)
        {
            return false;
        }

        session.warpTarget = destination;
        session.warpX = x;
        session.warpY = y;

        source->Commands().Push(LeaveCommand{connection});
        return true;
    }

    // Which simulation a connection is on, or kInvalidSimulationId. Mid
    // warp this is still the *source*, until the leave has landed.
    SimulationId SimulationFor(ConnectionId connection) const
    {
        const std::shared_lock<std::shared_mutex> lock(m_mutex);

        const auto it = m_sessions.find(connection);
        return it == m_sessions.end() ? kInvalidSimulationId : it->second.simulation;
    }

    std::size_t ConnectionCount() const
    {
        const std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_sessions.size();
    }

    // Routes one client's command to the simulation that client is on.
    //
    // Stamps the connection into the command so the two can never disagree,
    // then hands it to that simulation's queue. Nothing is resolved to an
    // Entity here -- see the PlayerCommand concept for why that has to
    // happen on the other side.
    //
    // False means there was nowhere to send it: unknown connection, or a
    // warp in flight with the client between two maps.
    template <PlayerCommand T>
    bool Send(ConnectionId connection, T command)
    {
        const std::shared_lock<std::shared_mutex> lock(m_mutex);

        const auto it = m_sessions.find(connection);
        if (it == m_sessions.end() || it->second.warpTarget != kInvalidSimulationId)
        {
            return false;
        }

        Simulation* target = FindLocked(it->second.simulation);
        if (target == nullptr)
        {
            return false;
        }

        command.connection = connection;
        target->Commands().Push(std::move(command));
        return true;
    }

    // --- driving ------------------------------------------------------

    // Ticks every simulation, then settles the routing table against what
    // the ticks did.
    //
    // Simulations are ticked in creation order, on this thread. They share
    // nothing, so ticking them on a pool later needs no locking here --
    // only that the bookkeeping below still happens once all of them have
    // finished, because it reads their routing tables between ticks.
    void Tick(float deltaSeconds)
    {
        assert(!m_ticking && "world_v2: World::Tick re-entered or ticked concurrently");
        m_ticking = true;

        for (const std::unique_ptr<Simulation>& simulation : m_simulations)
        {
            simulation->Tick(deltaSeconds);
        }

        // Order matters. Deaths first, so a connection whose character just
        // died is out of the table before anything tries to advance its
        // warp; arrivals and warps after, on what is left.
        std::vector<Drop> drops;
        CollectDeaths(drops);
        AdvanceSessions(drops);

        // Outside the lock, so a handler is free to Join the player
        // somewhere else without deadlocking on the table it was just
        // removed from.
        if (m_dropped)
        {
            for (const Drop& drop : drops)
            {
                m_dropped(drop.connection, drop.simulation);
            }
        }

        m_ticking = false;
    }

private:
    struct Session
    {
        SimulationId simulation = kInvalidSimulationId;
        CharacterId character = kInvalidCharacter;

        // Set once the simulation confirms the client actually has an
        // entity there. Everything that would move the client somewhere
        // else waits for this, so nothing can overtake the join that put
        // them here.
        bool arrived = false;

        // Ticks spent waiting for that confirmation. A join can be refused
        // -- an occupied tile, a full map -- and a refusal is silent from
        // out here, so it is inferred from never arriving rather than left
        // as a session that waits forever.
        int waiting = 0;

        // A warp in flight: the leave has gone out to `simulation`, and the
        // join to here has not. kInvalidSimulationId when not warping.
        SimulationId warpTarget = kInvalidSimulationId;
        int warpX = 0;
        int warpY = 0;
    };

    struct Drop
    {
        ConnectionId connection = kInvalidConnection;
        SimulationId simulation = kInvalidSimulationId;
    };

    // A join drains at the top of the very next tick, so one tick would do.
    // Two, because the cost of being wrong in each direction is lopsided:
    // waiting an extra tick costs a client nothing, while giving up too
    // early would report a player as dropped who is standing right there.
    static constexpr int kJoinGraceTicks = 2;

    Simulation* FindLocked(SimulationId id) const
    {
        const auto it = m_index.find(id);
        return it == m_index.end() ? nullptr : it->second;
    }

    // Connections whose entity stopped existing during the tick without a
    // leave. Each simulation already worked this out for its own clients;
    // this is only turning it into a routing decision.
    void CollectDeaths(std::vector<Drop>& drops)
    {
        const std::unique_lock<std::shared_mutex> lock(m_mutex);

        for (const std::unique_ptr<Simulation>& simulation : m_simulations)
        {
            for (const ConnectionId connection : simulation->DroppedConnections())
            {
                const auto it = m_sessions.find(connection);

                // The id check matters: a connection that left this
                // simulation and joined another one in the same tick is a
                // different session now, and dropping it here would strand
                // a client that is perfectly fine somewhere else.
                if (it == m_sessions.end() || it->second.simulation != simulation->Id())
                {
                    continue;
                }

                m_sessions.erase(it);
                drops.push_back(Drop{connection, simulation->Id()});
            }
        }
    }

    // The one sweep that moves sessions forward: notice who has arrived,
    // give up on who never will, and push the second half of a warp whose
    // first half has landed.
    //
    // All three read a simulation's routing table directly, which is only
    // legal because this runs between ticks, with nothing else touching it.
    void AdvanceSessions(std::vector<Drop>& drops)
    {
        const std::unique_lock<std::shared_mutex> lock(m_mutex);

        for (auto it = m_sessions.begin(); it != m_sessions.end();)
        {
            const ConnectionId connection = it->first;
            Session& session = it->second;

            Simulation* current = FindLocked(session.simulation);
            const bool present = current != nullptr && current->EntityFor(connection) != kNullEntity;

            if (!session.arrived)
            {
                if (present)
                {
                    session.arrived = true;
                    session.waiting = 0;
                    ++it;
                    continue;
                }

                if (++session.waiting < kJoinGraceTicks)
                {
                    ++it;
                    continue;
                }

                // Never showed up: the spawn callback refused, or that
                // simulation is not being ticked. Either way there is no
                // client there to route to.
                drops.push_back(Drop{connection, session.simulation});
                it = m_sessions.erase(it);
                continue;
            }

            if (session.warpTarget == kInvalidSimulationId)
            {
                ++it;
                continue;
            }

            // Still standing on the old map: its leave has not drained yet.
            // Nothing to do but wait for the tick that removes them.
            if (present)
            {
                ++it;
                continue;
            }

            Simulation* target = FindLocked(session.warpTarget);
            assert(target != nullptr && "world_v2: warp target vanished; simulations are never removed");

            target->Commands().Push(JoinCommand{connection, session.character, session.warpX, session.warpY});

            session.simulation = session.warpTarget;
            session.warpTarget = kInvalidSimulationId;

            // Arriving on the new map is a join like any other, and waits
            // to be confirmed the same way.
            session.arrived = false;
            session.waiting = 0;
            ++it;
        }
    }

    // Stable for the life of the World: appended to during setup only, and
    // each Simulation is behind a unique_ptr, so the pointers in m_index
    // and the ones captured by the notice sinks stay valid as it grows.
    std::vector<std::unique_ptr<Simulation>> m_simulations;
    std::unordered_map<SimulationId, Simulation*> m_index;

    mutable std::shared_mutex m_mutex;
    std::unordered_map<ConnectionId, Session> m_sessions;

    NoticeRouter m_router;
    DropHandler m_dropped;

    bool m_ticking = false;
};

} // namespace world_v2
