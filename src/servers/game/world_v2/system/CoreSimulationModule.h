#pragma once

#include "../core/Module.h"
#include "AISystem.h"
#include "CombatSystem.h"
#include "DeathSystem.h"
#include "DespawnSystem.h"
#include "GridMovementSystem.h"
#include "PickupSystem.h"
#include "SpawnSystem.h"

namespace world_v2
{

// The seven systems every map runs, in the one order they work in.
//
// This module exists to keep that order readable. Stage 2 is a loop now, so
// there is no longer a place in Simulation::Tick where the sequence can be
// seen and reasoned about -- and the sequence is the most load-bearing
// unwritten thing in the framework. It lives here instead, as seven
// consecutive AddSystem calls with the argument for each one.
//
// The order is a dependency chain, not a preference. AI writes the intents
// and attack requests; movement resolves the intents, which is what decides
// who is standing where; combat then checks reach against those settled
// positions; death notices what combat left at zero. Running any of them
// earlier would have it act on last tick's answer.
//
// Spawning leads because it only counts and emits -- the monsters it asks
// for are created at the barrier, so they first draw breath at the end of
// this tick and first act in the next one.
//
// Pickup sits after movement for the same reason combat does: reach has to
// be judged against where things actually ended up, not where they were
// when the packet arrived.
//
// Despawn is last, and after death on purpose. Both end in a despawn at the
// barrier, and something that dies on the same tick its timer runs out
// should be reported as having died -- the death handler is the one that
// credits a killer and drops loot, and an expiry that got there first would
// announce the removal with none of that.
//
// The After<T> declarations below say all of that a second time, in a form
// the machine checks. They do not reorder anything: install this module
// before anything that depends on it, and if some other arrangement ever
// puts these systems out of sequence, a debug build says so at Start rather
// than a monster quietly swinging at where somebody used to be.
//
// Taking a system out means not installing this module and adding the ones
// you want by hand -- which is the point. A map with no combat should not
// be paying for CombatSystem's view every tick.
class CoreSimulationModule : public Module
{
public:
    const char* Name() const override
    {
        return "CoreSimulation";
    }

    void Setup(ModuleContext& context) override
    {
        m_spawn = &context.AddSystem<SpawnSystem>().Get();

        m_ai = &context.AddSystem<AISystem>().Get();

        m_movement = &context.AddSystem<GridMovementSystem>().After<AISystem>().Get();

        m_pickup = &context.AddSystem<PickupSystem>().After<GridMovementSystem>().Get();

        m_combat = &context.AddSystem<CombatSystem>().After<GridMovementSystem>().Get();

        m_death = &context.AddSystem<DeathSystem>().After<CombatSystem>().Get();

        m_despawn = &context.AddSystem<DespawnSystem>().After<DeathSystem>().Get();
    }

    // Accessors for the three systems that carry tunables, so a game layer
    // can reach knobs that nothing routed through a Simulation could touch
    // before: AISystem::defaultAttackDamage, GridMovementSystem's
    // tilesPerSecond, PickupSystem::pickupRange.
    //
    //     auto& core = simulation.Install<CoreSimulationModule>();
    //     core.Movement().tilesPerSecond = 6.0f;
    SpawnSystem& Spawn() const
    {
        return *m_spawn;
    }

    AISystem& AI() const
    {
        return *m_ai;
    }

    GridMovementSystem& Movement() const
    {
        return *m_movement;
    }

    PickupSystem& Pickup() const
    {
        return *m_pickup;
    }

    CombatSystem& Combat() const
    {
        return *m_combat;
    }

    DeathSystem& Death() const
    {
        return *m_death;
    }

    DespawnSystem& Despawn() const
    {
        return *m_despawn;
    }

private:
    // Non-owning: Simulation owns the systems, this module only added them.
    SpawnSystem* m_spawn = nullptr;
    AISystem* m_ai = nullptr;
    GridMovementSystem* m_movement = nullptr;
    PickupSystem* m_pickup = nullptr;
    CombatSystem* m_combat = nullptr;
    DeathSystem* m_death = nullptr;
    DespawnSystem* m_despawn = nullptr;
};

} // namespace world_v2
