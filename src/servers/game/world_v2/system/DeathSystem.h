#pragma once

#include "../component/Combat.h"
#include "../component/Grid.h"
#include "../core/Entity.h"
#include "../core/EventManager.h"
#include "../core/Registry.h"
#include "../event/CombatEvents.h"

namespace world_v2
{

// Announces entities whose health has run out, and nothing else.
//
// It does not despawn them, drop their loot, or award anything. All of that
// is structural or cascading, and both belong at the barrier -- this runs
// inside the simulation stage, where the pools it is walking are the ones a
// despawn would reorder. So it emits a DeathEvent and moves on.
//
// The DeadComponent tag it leaves behind is what makes that split safe. A
// dead entity stays on the map, at zero health, until the barrier clears
// it, and every system that runs in between has to be able to tell it apart
// from a living one. Without the tag, a second death pass would announce
// the same death twice -- and since the death handler drops loot and awards
// experience, that duplicates both.
//
// Health reaching zero is the only trigger, so this covers anything that
// can kill: a landed hit, a damage-over-time effect, a scripted event.
// Nothing has to remember to call it.
class DeathSystem
{
public:
    void Update(Registry& registry, EventManager& events)
    {
        registry.view<HealthComponent>().Each(
            [&](Entity entity, HealthComponent& health)
            {
                if (health.current > 0 || registry.Has<DeadComponent>(entity))
                {
                    return;
                }

                // Overkill is clamped here rather than at every damage
                // source, so nothing downstream has to cope with negative
                // health.
                health.current = 0;

                Entity killer = kNullEntity;
                if (const LastAttackerComponent* last = registry.TryGet<LastAttackerComponent>(entity))
                {
                    // The killer may itself have died earlier in this same
                    // tick; the handler checks Exists before crediting it.
                    killer = last->attacker;
                }

                // Where it fell, captured before anything can remove it --
                // the standard handler for this event is the one that
                // despawns the corpse.
                int x = 0;
                int y = 0;
                if (const GridPositionComponent* position = registry.TryGet<GridPositionComponent>(entity))
                {
                    x = position->x;
                    y = position->y;
                }

                // Assigning to the entity currently being visited, on a
                // pool nothing is iterating.
                registry.Assign<DeadComponent>(entity);

                events.Emit(DeathEvent{entity, killer, x, y});
            });
    }
};

} // namespace world_v2
