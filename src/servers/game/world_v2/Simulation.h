#pragma once

#include "CommandQueue.h"
#include "Ids.h"
#include "component/Network.h"
#include "core/Map.h"
#include "core/Module.h"
#include "core/System.h"
#include "core/TypeId.h"
#include "system/BroadcastSystem.h"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <typeinfo>
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
using PlayerSpawner = std::function<Entity(Map&, const JoinCommand&)>;

// A command that came from a particular client.
//
// The connection is the only identity a socket thread can safely put in a
// command. It cannot look up the sender's Entity itself: the table that
// answers that question is owned by the simulation thread and is being
// rewritten by joins, leaves, and deaths while the packet is in flight. So
// the command carries the durable id, and the resolution happens where the
// answer is stable -- see ModuleContext::OnPlayerCommand.
template <typename T>
concept PlayerCommand = requires(const T& command) {
    { command.connection } -> std::convertible_to<ConnectionId>;
};

// Takes a leaving client's entity out of the world. Defaults to a plain
// Map::Despawn; override it when leaving has to save state, drop a
// corpse, or hand carried items somewhere first.
using PlayerDespawner = std::function<void(Map&, Entity)>;

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
// Stages 2 and 4 are supplied by modules. Simulation owns the *shape* of
// the tick -- the drain, the barrier, the reap, and their order -- and
// knows nothing about what runs inside stages 2 and 4. See core/Module.h.
//
// Modules
// -------
// Everything a simulation does beyond that shape arrives through Install:
//
//     simulation.Install<CoreSimulationModule>();
//     simulation.Install<CombatRulesModule>();
//     simulation.Install<MyFeatureModule>(someConfig);
//
// Each module's Setup runs immediately, in install order, so a stage-2
// system runs where its module put it in the list. Once the first Tick
// arrives the simulation seals: every module's Start runs, and nothing more
// may be registered. That last part used to be a comment on five separate
// methods and was enforced by none of them.
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
        // Reserved: JoinCommand and LeaveCommand are the two command types
        // Simulation handles itself, and a module registering either would
        // quietly break routing. Claiming them here means that attempt now
        // trips the duplicate-owner assertion instead of succeeding.
        // Anything a game layer wants to happen on join belongs in the
        // PlayerSpawner.
        ClaimCommand<JoinCommand>("Simulation");
        m_commands.On<JoinCommand>([this](Map& world, const JoinCommand& command) {
            ApplyJoin(world, command);
        });

        ClaimCommand<LeaveCommand>("Simulation");
        m_commands.On<LeaveCommand>([this](Map& world, const LeaveCommand& command) {
            ApplyLeave(world, command);
        });
    }

    ~Simulation()
    {
        // Reverse install order, so a module tears down before anything it
        // was installed on top of.
        for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it)
        {
            (*it)->Shutdown();
        }
    }

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    // Installs a module and runs its Setup immediately, returning it so the
    // caller can keep a reference for configuration or lookup.
    //
    // Setup runs now rather than being deferred to Start because that is
    // what makes install order and declaration order the same thing: the
    // systems a module adds land in the tick list at the point it was
    // installed, with nothing in between reordering them.
    template <typename M, typename... Args>
    M& Install(Args&&... args)
    {
        static_assert(std::is_base_of_v<Module, M>, "world_v2: Install requires a Module subclass");
        assert(!m_started && "world_v2: Simulation::Install after the first Tick");

        auto owned = std::make_unique<M>(std::forward<Args>(args)...);
        M& module = *owned;
        m_modules.push_back(std::move(owned));

        ModuleContext context(*this, m_world, module.Name());
        module.Setup(context);

        return module;
    }

    // The installed module of type M, or nullptr. How modules reach each
    // other without a global singleton.
    template <typename M>
    M* Find()
    {
        for (const std::unique_ptr<Module>& module : m_modules)
        {
            if (M* found = dynamic_cast<M*>(module.get()))
            {
                return found;
            }
        }

        return nullptr;
    }

    // The installed stage-2 system of type T, or nullptr. Same idea, for
    // the case where one module needs to reach a system another module
    // owns -- SpawnRulesModule priming CoreSimulationModule's SpawnSystem,
    // for instance.
    template <typename T>
    T* FindSystem()
    {
        for (const std::unique_ptr<ISystem>& system : m_systems)
        {
            if (T* found = dynamic_cast<T*>(system.get()))
            {
                return found;
            }
        }

        return nullptr;
    }

    // Runs every module's Start and seals the simulation. Idempotent, and
    // called by the first Tick, so a caller only needs it explicitly when
    // the world must be fully primed before anything else looks at it.
    void Start()
    {
        if (m_started)
        {
            return;
        }

        // Set first: Start runs with the simulation already sealed, so a
        // module that tries to register from Start trips the assertion
        // rather than appending to a list the tick is about to walk.
        m_started = true;

        VerifySystemOrder();

        for (const std::unique_ptr<Module>& module : m_modules)
        {
            module->Start(*this);
        }
    }

    SimulationId Id() const
    {
        return m_id;
    }

    Map& World()
    {
        return m_world;
    }

    // The inbound seam. Safe to touch from any thread -- see CommandQueue.
    CommandQueue& Commands()
    {
        return m_commands;
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

    // Stage 4. Called once per (viewer, notice) pair that the viewer can
    // see, after the world has settled.
    //
    // Stays on Simulation rather than moving to a module because it is the
    // outbound half of routing, symmetric with Commands() being the inbound
    // half -- World installs it when it creates a simulation, and whichever
    // module builds notices reads it back through NoticeSink().
    void OnNotice(NoticeSink sink)
    {
        assert(!m_started && "world_v2: Simulation::OnNotice after the first Tick");
        m_sink = std::move(sink);
    }

    const NoticeSink& Sink() const
    {
        return m_sink;
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

        // Seals the simulation and primes the world, once. A caller that
        // needs the world populated before the first tick calls Start
        // itself; everyone else gets it here.
        Start();

        m_ticking = true;

        // --- 1. Inbound -----------------------------------------------
        m_lastCommandCount = m_commands.Drain(m_world);

        // --- 2. Simulation --------------------------------------------
        // In the order the modules declared them. The dependency chain
        // among the built-in seven is documented where they are installed,
        // in CoreSimulationModule -- that is the one place it can be read
        // as a sequence now that this is a loop.
        for (const std::unique_ptr<ISystem>& system : m_systems)
        {
            system->Run(m_world, deltaSeconds);
        }

        // --- 3. Barrier -----------------------------------------------
        m_world.events.Flush();

        // Straight after, and before anything reads the settled world: a
        // player who died this tick is gone as of the flush above, and the
        // routing table must not still be claiming otherwise when the
        // outbound stage starts asking who is who.
        ReapDroppedPlayers();

        // --- 4. Outbound ----------------------------------------------
        // Nothing here modifies anything: the tick's story is finished, and
        // the job is to describe it.
        for (const std::function<void(Map&)>& outbound : m_outbound)
        {
            outbound(m_world);
        }

        m_ticking = false;
    }

private:
    friend class ModuleContext;

    template <typename T>
    friend class SystemHandle;

    // A system's place in the tick list, remembered only so After<T> has
    // something to check against. Debug builds only.
    struct OrderConstraint
    {
        TypeId self = 0;
        TypeId required = 0;
        const char* selfName = "";
        const char* requiredName = "";
        const char* moduleName = "";
    };

    template <typename T>
    void ClaimCommand(const char* owner)
    {
        const TypeId id = TypeIdOf<CommandFamily>::Value<T>();
        const auto existing = m_commandOwners.find(id);

        // One handler per command type is CommandQueue's deliberate rule --
        // two independent readings of the same inbound packet is a wiring
        // mistake. It silently replaced before, which was survivable when
        // all the wiring lived in one function and is not now that two
        // modules can each think they own a command.
        assert(existing == m_commandOwners.end() &&
               "world_v2: two modules claim the same command type -- see the owner names below");
        if (existing != m_commandOwners.end())
        {
            // Kept out of the assertion text so the names are visible in a
            // release build's debugger too.
            (void)existing->second;
            (void)owner;
            return;
        }

        m_commandOwners.emplace(id, owner);
    }

    void VerifySystemOrder()
    {
#ifndef NDEBUG
        for (const OrderConstraint& constraint : m_constraints)
        {
            const std::size_t self = IndexOfSystem(constraint.self);
            const std::size_t required = IndexOfSystem(constraint.required);

            assert(self != kNoSystem && "world_v2: After<T> recorded for a system that is not installed");
            assert(required != kNoSystem &&
                   "world_v2: a module declared After<T> on a system nothing installed -- see requiredName");
            assert(required < self &&
                   "world_v2: declared system order violates After<T> -- install the required module first");

            (void)self;
            (void)required;
        }
#endif
    }

    static constexpr std::size_t kNoSystem = static_cast<std::size_t>(-1);

    std::size_t IndexOfSystem(TypeId id) const
    {
        for (std::size_t index = 0; index < m_systemTypes.size(); ++index)
        {
            if (m_systemTypes[index] == id)
            {
                return index;
            }
        }

        return kNoSystem;
    }

    // Stage 1, from the command queue. Everything structural about a join
    // happens here, on the simulation thread, before any system runs.
    void ApplyJoin(Map& world, const JoinCommand& command)
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

    void ApplyLeave(Map& world, const LeaveCommand& command)
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

    Map m_world;
    CommandQueue m_commands;

    std::vector<std::unique_ptr<Module>> m_modules;

    // Stage 2, in declaration order. m_systemTypes is the parallel type
    // list After<T> checks against -- kept beside rather than inside
    // ISystem so a system needs no knowledge of the ordering machinery.
    std::vector<std::unique_ptr<ISystem>> m_systems;
    std::vector<TypeId> m_systemTypes;
    std::vector<OrderConstraint> m_constraints;

    // Stage 4, in declaration order. Multicast, unlike the single callback
    // slot it replaces: several modules can describe one settled world.
    std::vector<std::function<void(Map&)>> m_outbound;

    // Which module claimed each command type, so the duplicate assertion
    // can name both sides.
    std::unordered_map<TypeId, const char*> m_commandOwners;

    NoticeSink m_sink;

    PlayerSpawner m_spawnPlayer;
    PlayerDespawner m_despawnPlayer;
    const char* m_spawnPlayerOwner = nullptr;
    const char* m_despawnPlayerOwner = nullptr;

    // connection -> Entity. The other direction is PlayerSessionComponent.
    std::unordered_map<ConnectionId, Entity> m_connections;
    std::vector<ConnectionId> m_dropped;

    std::size_t m_rejectedJoins = 0;
    std::size_t m_orphanedCommands = 0;
    std::size_t m_lastCommandCount = 0;
    bool m_ticking = false;
    bool m_started = false;
};

// --------------------------------------------------------------------
// ModuleContext, now that Simulation is a complete type.
//
// Every one of these asserts !m_started. That is the whole reason the
// context exists as a separate object rather than as methods on Simulation:
// registration is a thing you may only do during Setup, and the type system
// plus one assertion say so.
// --------------------------------------------------------------------

template <typename T, typename... Args>
SystemHandle<T> ModuleContext::AddSystem(Args&&... args)
{
    static_assert(std::is_base_of_v<ISystem, T>, "world_v2: AddSystem requires an ISystem subclass");

    Simulation& simulation = *m_simulation;
    assert(!simulation.m_started && "world_v2: AddSystem after the first Tick");

    auto owned = std::make_unique<T>(std::forward<Args>(args)...);
    T& system = *owned;

    simulation.m_systems.push_back(std::move(owned));
    simulation.m_systemTypes.push_back(TypeIdOf<SystemFamily>::Value<T>());

    return SystemHandle<T>(system, *this);
}

template <typename E>
void ModuleContext::Listen(std::function<void(const E&)> handler)
{
    assert(!m_simulation->m_started && "world_v2: Listen after the first Tick");
    m_world->events.Listen<E>(std::move(handler));
}

template <typename T>
void ModuleContext::OnCommand(std::function<void(Map&, const T&)> handler)
{
    Simulation& simulation = *m_simulation;
    assert(!simulation.m_started && "world_v2: OnCommand after the first Tick");

    simulation.ClaimCommand<T>(m_moduleName);
    simulation.m_commands.On<T>(std::move(handler));
}

template <typename T>
void ModuleContext::OnPlayerCommand(std::function<void(Map&, Entity, const T&)> handler)
{
    static_assert(PlayerCommand<T>, "world_v2: OnPlayerCommand requires a command carrying a connection");

    Simulation& simulation = *m_simulation;
    assert(!simulation.m_started && "world_v2: OnPlayerCommand after the first Tick");

    simulation.ClaimCommand<T>(m_moduleName);

    // The resolution a game layer must not do by hand: the sender may have
    // left, warped, or died between the push and this drain, and a stale
    // handle would land the write on whoever inherited the slot.
    simulation.m_commands.On<T>(
        [&simulation, handler = std::move(handler)](Map& world, const T& command)
        {
            const Entity actor = simulation.EntityFor(command.connection);
            if (actor == kNullEntity)
            {
                ++simulation.m_orphanedCommands;
                return;
            }

            handler(world, actor, command);
        });
}

inline void ModuleContext::AddOutbound(std::function<void(Map&)> handler)
{
    assert(!m_simulation->m_started && "world_v2: AddOutbound after the first Tick");
    m_simulation->m_outbound.push_back(std::move(handler));
}

inline void ModuleContext::OnSpawnPlayer(std::function<Entity(Map&, const JoinCommand&)> spawner)
{
    Simulation& simulation = *m_simulation;
    assert(!simulation.m_started && "world_v2: OnSpawnPlayer after the first Tick");
    assert(simulation.m_spawnPlayerOwner == nullptr &&
           "world_v2: two modules both define how a player spawns");

    simulation.m_spawnPlayerOwner = m_moduleName;
    simulation.m_spawnPlayer = std::move(spawner);
}

inline void ModuleContext::OnDespawnPlayer(std::function<void(Map&, Entity)> despawner)
{
    Simulation& simulation = *m_simulation;
    assert(!simulation.m_started && "world_v2: OnDespawnPlayer after the first Tick");
    assert(simulation.m_despawnPlayerOwner == nullptr &&
           "world_v2: two modules both define how a player despawns");

    simulation.m_despawnPlayerOwner = m_moduleName;
    simulation.m_despawnPlayer = std::move(despawner);
}

template <typename T>
template <typename U>
SystemHandle<T>& SystemHandle<T>::After()
{
#ifndef NDEBUG
    static_assert(std::is_base_of_v<ISystem, U>, "world_v2: After<T> requires an ISystem subclass");

    Simulation& simulation = *m_context->m_simulation;
    simulation.m_constraints.push_back(Simulation::OrderConstraint{
        TypeIdOf<SystemFamily>::Value<T>(), TypeIdOf<SystemFamily>::Value<U>(), typeid(T).name(), typeid(U).name(),
        m_context->ModuleName()});
#endif

    return *this;
}

} // namespace world_v2
