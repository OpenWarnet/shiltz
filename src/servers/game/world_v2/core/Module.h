#pragma once

#include "Map.h"
#include "System.h"
#include "TypeId.h"

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace world_v2
{

class Simulation;
class ModuleContext;

// A separate numbering for system types, so After<T> can name one without
// borrowing the component or event families.
struct SystemFamily
{
};

// A unit of behaviour installed into a Simulation.
//
// The whole point is that adding a feature stops meaning "edit
// Simulation.h". A module declares what it registers, gets installed by
// name, and Simulation never learns what it does.
//
//     class LootModule final : public Module
//     {
//     public:
//         const char* Name() const override { return "Loot"; }
//
//         void Setup(ModuleContext& ctx) override
//         {
//             ctx.Listen<LootDropEvent>([&ctx](const LootDropEvent& event) {
//                 SpawnGroundItem(ctx.World(), event.dropTableId, 1, 10, event.x, event.y);
//             });
//         }
//     };
//
//     simulation.Install<LootModule>();
//
// Setup / Start
// -------------
// Setup registers. Start runs only once *every* installed module has been
// set up, which is what makes it safe to do work that depends on another
// module's listeners being live.
//
// That split is not decoration -- it retires a real footgun. Priming the
// spawners used to mean calling PrimeSpawns() by hand, after installing the
// spawn rules, after placing the spawner entities, in that order, with
// nothing but WorldRunner's call sequence saying so. Now the priming lives
// in SpawnRulesModule::Start and simply cannot run before the listeners it
// needs exist, whatever order anything was installed in.
//
// Ordering
// --------
// Install order is the contract. Modules are set up in the order they were
// installed, and within one Setup, registrations take effect in call order.
// A stage-2 system therefore runs where its module put it in the list.
//
// The cost of that simplicity is that installing CombatModule before
// MovementModule silently makes combat judge reach against last tick's
// positions. See ModuleContext::AddSystem and SystemHandle::After for the
// debug-only check that catches exactly that.
class Module
{
public:
    virtual ~Module() = default;

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    // Appears in assertion messages -- "MyModule and Combat both claim
    // AttackCommand" is only useful if both names are real.
    virtual const char* Name() const = 0;

    // Register systems, commands, listeners and outbound builders. Called
    // once, in install order, before any module's Start.
    virtual void Setup(ModuleContext& context)
    {
        (void)context;
    }

    // Runs after every module's Setup. Nothing may be registered here --
    // the simulation is sealed by this point -- but the world can be read
    // and written, which is what priming needs.
    virtual void Start(Simulation& simulation)
    {
        (void)simulation;
    }

    // Teardown, in reverse install order, when the Simulation is destroyed.
    virtual void Shutdown()
    {
    }

protected:
    Module() = default;
};

// What AddSystem hands back: the system itself, for tuning, and a place to
// declare what it expects to run after.
template <typename T>
class SystemHandle
{
public:
    SystemHandle(T& system, ModuleContext& context)
        : m_system(&system)
        , m_context(&context)
    {
    }

    // The system instance, so a module can set the knobs that used to be
    // unreachable through a Simulation:
    //
    //     ctx.AddSystem<AISystem>().Get().defaultAttackDamage = 3;
    T& Get() const
    {
        return *m_system;
    }

    operator T&() const
    {
        return *m_system;
    }

    T* operator->() const
    {
        return m_system;
    }

    // Declares that this system expects U to have run before it, in the
    // same tick.
    //
    // It does NOT reorder anything. Declaration order stays the whole
    // contract; this only checks that the order you declared actually
    // satisfies what you said you needed, and asserts at Start if it does
    // not. Sorting would make the effective tick order invisible at the
    // call site, which is the property this design is keeping.
    //
    // Compiled away entirely under NDEBUG -- the call still exists and
    // still returns *this, so release builds see the same source.
    template <typename U>
    SystemHandle& After();

private:
    T* m_system;
    ModuleContext* m_context;
};

// The registration surface handed to Module::Setup.
//
// There is no phase enum, because the method you call *is* the phase. The
// four stages of Simulation::Tick each get exactly one way in, and none of
// them can reach inside the barrier:
//
//   OnCommand / OnPlayerCommand   stage 1, inbound
//   AddSystem                     stage 2, the simulation sweep
//   Listen                        stage 3, during the flush
//   AddOutbound                   stage 4, describing the settled world
//
// Everything here is setup-only and asserts it. That used to be a comment
// on five different methods and was enforced by none of them.
class ModuleContext
{
public:
    ModuleContext(Simulation& simulation, Map& world, const char* moduleName)
        : m_simulation(&simulation)
        , m_world(&world)
        , m_moduleName(moduleName)
    {
    }

    ModuleContext(const ModuleContext&) = delete;
    ModuleContext& operator=(const ModuleContext&) = delete;

    // Direct access, for authoring rather than registration: carving
    // terrain, placing spawner entities, anything that is world data rather
    // than behaviour.
    Map& World() const
    {
        return *m_world;
    }

    Simulation& Sim() const
    {
        return *m_simulation;
    }

    const char* ModuleName() const
    {
        return m_moduleName;
    }

    // Stage 2. Constructs the system in place, appends it to the tick list,
    // and returns a handle for tuning and for After<U>().
    template <typename T, typename... Args>
    SystemHandle<T> AddSystem(Args&&... args);

    // Stage 3. Multicast, in registration order, exactly as
    // EventManager::Listen already behaves -- this is a pass-through that
    // exists so a module never has to reach through World() to subscribe.
    template <typename E>
    void Listen(std::function<void(const E&)> handler);

    // Stage 1, raw. One handler per command type: registering a type twice
    // is a wiring mistake rather than a feature, and now it asserts and
    // names both modules instead of silently replacing the first.
    template <typename T>
    void OnCommand(std::function<void(Map&, const T&)> handler);

    // Stage 1, resolved. Same one-per-type rule; the connection is turned
    // into the acting Entity first, and commands from a connection with no
    // live entity are counted and dropped.
    template <typename T>
    void OnPlayerCommand(std::function<void(Map&, Entity, const T&)> handler);

    // Stage 4. Multicast and in declaration order, unlike the single
    // OnBroadcast slot it replaces -- several modules can describe the same
    // settled world without fighting over one callback.
    void AddOutbound(std::function<void(Map&)> handler);

    // Single-slot, and asserts rather than replacing if two modules both
    // try to own how a player enters or leaves.
    void OnSpawnPlayer(std::function<Entity(Map&, const struct JoinCommand&)> spawner);
    void OnDespawnPlayer(std::function<void(Map&, Entity)> despawner);

private:
    template <typename T>
    friend class SystemHandle;

    Simulation* m_simulation;
    Map* m_world;
    const char* m_moduleName;
};

// A module without a class, for one-off wiring.
//
// Tests and small game-layer glue would otherwise need a named class per
// behaviour, which is worse than what it replaces. A real feature should
// still be a real Module -- this is for the cases where the name would
// carry no information.
class LambdaModule final : public Module
{
public:
    LambdaModule(std::string name, std::function<void(ModuleContext&)> setup,
                 std::function<void(Simulation&)> start = {})
        : m_name(std::move(name))
        , m_setup(std::move(setup))
        , m_start(std::move(start))
    {
    }

    const char* Name() const override
    {
        return m_name.c_str();
    }

    void Setup(ModuleContext& context) override
    {
        if (m_setup)
        {
            m_setup(context);
        }
    }

    void Start(Simulation& simulation) override
    {
        if (m_start)
        {
            m_start(simulation);
        }
    }

private:
    std::string m_name;
    std::function<void(ModuleContext&)> m_setup;
    std::function<void(Simulation&)> m_start;
};

} // namespace world_v2
