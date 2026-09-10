#pragma once

#include "../Simulation.h"
#include "../core/Module.h"
#include "BroadcastSystem.h"

namespace world_v2
{

// Turns the tick's events into notices, and hands each viewer the ones it
// can see.
//
// Broadcast collection used to be unconditional -- Simulation's constructor
// installed it, whether or not anything was watching -- because a
// simulation created and then forgotten would otherwise tick perfectly and
// tell nobody anything. As a module it is opt-in, which is the honest
// arrangement now that a map can be assembled piece by piece: a simulation
// with no outbound module is one nobody is listening to, and that is a
// statement the install list makes out loud rather than a silent default.
//
// The sink itself stays on Simulation. It is the outbound half of routing,
// installed by World when it creates a simulation, and read back here at
// delivery time -- so swapping this module for a different outbound model
// does not disturb how a notice reaches a connection.
class BroadcastModule : public Module
{
public:
    const char* Name() const override
    {
        return "Broadcast";
    }

    void Setup(ModuleContext& context) override
    {
        // Stage 3. Seven listeners that flatten events into notices; see
        // BroadcastSystem, which still owns the mapping.
        m_system.Install(context.World());

        // Stage 4. Reads the settled world and describes it. Deliver clears
        // the notice list with or without a sink, so an unwatched
        // simulation does not accumulate one.
        Simulation& simulation = context.Sim();
        context.AddOutbound([this, &simulation](Map& world) {
            m_system.Deliver(world.registry, simulation.Sink());
        });
    }

    // For tests and diagnostics -- reach it with
    // simulation.Find<BroadcastModule>()->System().
    BroadcastSystem& System()
    {
        return m_system;
    }

private:
    BroadcastSystem m_system;
};

} // namespace world_v2
