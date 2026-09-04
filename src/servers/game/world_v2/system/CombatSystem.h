#pragma once

#include "../component/Combat.h"
#include "../component/Grid.h"
#include "../component/Request.h"
#include "../core/Entity.h"
#include "../core/EventManager.h"
#include "../core/Registry.h"
#include "../event/CombatEvents.h"

#include <cstdlib>

namespace world_v2
{

// Resolves this tick's attack requests into health changes.
//
// Every request is consumed whether or not it lands, for the same reason
// movement consumes intents: a request that survived rejection would retry
// forever, and one that survived success would hit twice.
//
// Requests are re-validated here rather than trusted. AISystem checked
// range a moment ago, but the target may have stepped away since -- movement
// runs in between -- and a request that came from a client packet was never
// checked at all. Range, liveness, and existence are all confirmed against
// the state as it is now.
//
// Nothing here removes an entity. Health hitting zero is left standing for
// DeathSystem to announce and the barrier to resolve.
class CombatSystem
{
public:
    void Update(Registry& registry, EventManager& events)
    {
        registry.view<AttackRequestComponent>().Each(
            [&](Entity attacker, AttackRequestComponent& request)
            {
                const Entity target = request.targetEntity;
                const int damage = request.damage;

                // Read before consuming -- removing the component below
                // leaves `request` dangling.
                registry.Remove<AttackRequestComponent>(attacker);

                if (damage <= 0 || !registry.Exists(target) || target == attacker)
                {
                    return;
                }

                if (registry.Has<DeadComponent>(target))
                {
                    return;
                }

                if (!InReach(registry, attacker, target))
                {
                    return;
                }

                // Captured now, while the target is certainly still on the
                // map -- see DamageDealtEvent on why the event carries it.
                const GridPositionComponent& position = registry.Get<GridPositionComponent>(target);
                const int targetX = position.x;
                const int targetY = position.y;

                HealthComponent* health = registry.TryGet<HealthComponent>(target);
                if (health == nullptr || health->current <= 0)
                {
                    return;
                }

                health->current -= damage;
                if (health->current < 0)
                {
                    health->current = 0;
                }

                const int remaining = health->current;

                // Structural, but on a pool nothing is iterating -- the
                // driving pool here is AttackRequestComponent.
                registry.Assign<LastAttackerComponent>(target, attacker);

                events.Emit(DamageDealtEvent{attacker, target, damage, remaining, targetX, targetY});
            });
    }

private:
    // Both parties must still be on the grid, and within the attacker's
    // reach as it stands now.
    //
    // An attacker with no AIComponent -- a player, whose reach comes from
    // equipment the framework knows nothing about -- is allowed melee
    // range, which is the conservative reading. The game layer widens it by
    // giving the entity an AIComponent's attackRange or by validating the
    // packet before it ever becomes a request.
    static bool InReach(Registry& registry, Entity attacker, Entity target)
    {
        const GridPositionComponent* from = registry.TryGet<GridPositionComponent>(attacker);
        const GridPositionComponent* to = registry.TryGet<GridPositionComponent>(target);
        if (from == nullptr || to == nullptr)
        {
            return false;
        }

        const int dx = std::abs(from->x - to->x);
        const int dy = std::abs(from->y - to->y);
        const int distance = dx > dy ? dx : dy;

        const AIComponent* ai = registry.TryGet<AIComponent>(attacker);
        const int reach = ai != nullptr ? ai->attackRange : 1;

        return distance <= reach;
    }
};

} // namespace world_v2
