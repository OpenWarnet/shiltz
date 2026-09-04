#pragma once

#include "../component/Despawn.h"
#include "../component/Grid.h"
#include "../core/Entity.h"
#include "../core/EventManager.h"
#include "../core/Registry.h"
#include "../event/LifecycleEvents.h"

namespace world_v2
{

// Counts down despawn timers and announces the ones that have run out.
//
// Same shape as DeathSystem, and for the same reason: it decrements and
// emits, and nothing else. Removing the entity is structural, so it happens
// at the barrier -- see InstallDespawnRules.
//
// The ExpiredComponent tag is what makes that split safe. An expired entity
// stays on the map until the barrier clears it, so without the tag a second
// pass would announce the same expiry again, and anything a game rule hangs
// off an expiry would run twice.
//
// Timing note: the countdown is applied before the test, so a timer created
// with `remaining` at zero expires on the next tick rather than being
// treated as already expired. There is no way to construct one that never
// fires.
class DespawnSystem
{
public:
    void Update(Registry& registry, EventManager& events, float deltaSeconds)
    {
        registry.view<DespawnTimerComponent>().Each(
            [&](Entity entity, DespawnTimerComponent& timer)
            {
                if (registry.Has<ExpiredComponent>(entity))
                {
                    return;
                }

                timer.remaining -= deltaSeconds;
                if (timer.remaining > 0.0f)
                {
                    return;
                }

                // Clamped so nothing downstream has to cope with a negative
                // clock -- the same courtesy DeathSystem does for health.
                timer.remaining = 0.0f;

                // Where it was, captured before anything can remove it: the
                // standard handler for this event is the one that despawns.
                int x = 0;
                int y = 0;
                if (const GridPositionComponent* position = registry.TryGet<GridPositionComponent>(entity))
                {
                    x = position->x;
                    y = position->y;
                }

                // Assigning to the entity currently being visited, on a
                // pool nothing is iterating.
                registry.Assign<ExpiredComponent>(entity);

                events.Emit(EntityExpiredEvent{entity, x, y});
            });
    }
};

} // namespace world_v2
