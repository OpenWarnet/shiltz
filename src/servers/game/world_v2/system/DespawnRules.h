#pragma once

#include "../core/Map.h"
#include "../core/Module.h"
#include "../event/LifecycleEvents.h"

namespace world_v2
{

// The default resolution of what DespawnSystem announces.
//
// Game policy on top of the framework, in the same shape as CombatRules,
// SpawnRules and ItemRules: the system decides *that* something's time is
// up, this decides what becomes of it. A module rather than built in so a
// game can replace it -- a corpse that expires into a pile of bones, a
// summon that refunds mana on the way out -- without touching the system.
//
// Runs inside EventManager::Flush, at the barrier, which is the only point
// in the tick where despawning is legal.
class DespawnRulesModule : public Module
{
public:
    const char* Name() const override
    {
        return "DespawnRules";
    }

    void Setup(ModuleContext& context) override
    {
        Map& world = context.World();

        context.Listen<EntityExpiredEvent>(
            [&world](const EntityExpiredEvent& event)
            {
                // An earlier handler in this same flush may already have
                // removed it -- an item picked up on the very tick its
                // timer ran out is the ordinary case, not a rare one, since
                // pickup resolves earlier in the same tick.
                if (!world.registry.Exists(event.entity))
                {
                    return;
                }

                world.Despawn(event.entity);
            });
    }
};

} // namespace world_v2
