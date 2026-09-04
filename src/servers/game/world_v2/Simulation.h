#pragma once

#include "CommandQueue.h"
#include "system/AISystem.h"
#include "system/BroadcastSystem.h"
#include "system/CombatSystem.h"
#include "system/DeathSystem.h"
#include "system/GridMovementSystem.h"
#include "system/PickupSystem.h"
#include "system/SpawnSystem.h"
#include "world/MapWorld.h"

#include <cassert>
#include <cstddef>
#include <functional>

namespace world_v2
{

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
    Simulation(int width, int height, bool walkableByDefault = false)
        : m_world(width, height, walkableByDefault)
    {
        // Registered here rather than left to the caller so that a notice
        // can never be missed because collection was wired up after the
        // first tick.
        m_broadcast.Install(m_world);
    }

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    MapWorld& World()
    {
        return m_world;
    }

    // The inbound seam. Safe to touch from any thread -- see CommandQueue.
    CommandQueue& Commands()
    {
        return m_commands;
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

        // --- 3. Barrier -----------------------------------------------
        m_world.events.Flush();

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
    MapWorld m_world;
    CommandQueue m_commands;

    SpawnSystem m_spawn;
    AISystem m_ai;
    GridMovementSystem m_movement;
    PickupSystem m_pickup;
    CombatSystem m_combat;
    DeathSystem m_death;

    BroadcastSystem m_broadcast;
    NoticeSink m_sink;
    std::function<void(MapWorld&)> m_rawBroadcast;

    std::size_t m_lastCommandCount = 0;
    bool m_ticking = false;
};

} // namespace world_v2
