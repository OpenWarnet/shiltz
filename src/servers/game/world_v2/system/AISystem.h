#pragma once

#include "../component/Combat.h"
#include "../component/Grid.h"
#include "../component/Request.h"
#include "../core/Entity.h"
#include "../core/Registry.h"
#include "../world/TileGrid.h"

#include <cstdlib>

namespace world_v2
{

// Decides what each AI-driven entity wants to do this tick, and says so by
// writing a request the later systems act on -- a MoveIntentComponent to
// close distance, or an AttackRequestComponent when already in reach. It
// changes nothing about the world itself, which is why it can run first.
//
// Target search
// -------------
// Vision is a square scan of the occupancy grid around the entity, not a
// pass over every entity that exists. The cost is bounded by visionRange
// squared no matter how crowded the map gets -- 289 tile reads at range 8 --
// which is the property the grid was built for. A monster in a town with
// two thousand players costs exactly what it costs in an empty field.
//
// Only blocking entities are in that grid, which lines up with what should
// be targetable: creatures block, ground items do not, and nothing wants to
// attack a dropped potion.
//
// Distances are Chebyshev -- the largest of the two axis deltas -- because
// movement allows diagonal steps at the same cost as straight ones. Using
// anything else would mean the AI and the map disagreed about how far away
// something is.
//
// Ties are broken by scan order (row by row, left to right) and the first
// entity found at the smallest distance wins. That is arbitrary but fixed,
// so two runs of the same state pick the same target.
//
// Not modelled yet: line of sight. A monster can currently see through a
// wall, because TileGrid knows which tiles block movement but nothing
// traces a ray across them.
class AISystem
{
public:
    // Used when an attacker has no AttackPowerComponent of its own.
    int defaultAttackDamage = 1;

    void Update(Registry& registry, const TileGrid& tiles)
    {
        registry.view<AIComponent, GridPositionComponent, FactionComponent>().Each(
            [&](Entity self, AIComponent& ai, GridPositionComponent& position, FactionComponent& faction)
            {
                // A corpse still on the map until the barrier clears it
                // does not get to keep making decisions.
                if (registry.Has<DeadComponent>(self))
                {
                    return;
                }

                const int selfX = position.x;
                const int selfY = position.y;

                // Last tick's target may have died, been despawned, or
                // walked out of sight.
                if (!IsEngageable(registry, faction.faction, ai.currentTarget) ||
                    Distance(registry, selfX, selfY, ai.currentTarget) > ai.visionRange)
                {
                    ai.currentTarget = kNullEntity;
                }

                if (ai.currentTarget == kNullEntity)
                {
                    ai.currentTarget = FindNearest(registry, tiles, self, faction.faction, selfX, selfY, ai.visionRange);
                }

                if (ai.currentTarget == kNullEntity)
                {
                    return;
                }

                const GridPositionComponent& targetPosition = registry.Get<GridPositionComponent>(ai.currentTarget);
                const int targetX = targetPosition.x;
                const int targetY = targetPosition.y;
                const int distance = Chebyshev(selfX, selfY, targetX, targetY);

                // Both branches assign to the entity this view is currently
                // visiting, which View documents as safe -- and both go to
                // pools other than the ones being iterated, so the
                // references above stay valid.
                if (distance <= ai.attackRange)
                {
                    registry.Assign<AttackRequestComponent>(self, ai.currentTarget, DamageOf(registry, self));
                    return;
                }

                registry.Assign<MoveIntentComponent>(self, Step(targetX - selfX), Step(targetY - selfY));
            });
    }

private:
    static int Chebyshev(int ax, int ay, int bx, int by)
    {
        const int dx = std::abs(ax - bx);
        const int dy = std::abs(ay - by);
        return dx > dy ? dx : dy;
    }

    // -1, 0 or +1: one tile's worth of movement along an axis.
    static int Step(int delta)
    {
        return delta > 0 ? 1 : (delta < 0 ? -1 : 0);
    }

    // A living entity of another faction that is still on the map.
    //
    // An entity with no FactionComponent is not engageable: nobody has said
    // whose side it is on, and guessing is worse than ignoring it.
    static bool IsEngageable(Registry& registry, int selfFaction, Entity candidate)
    {
        if (candidate == kNullEntity || !registry.Exists(candidate))
        {
            return false;
        }

        if (registry.Has<DeadComponent>(candidate))
        {
            return false;
        }

        const FactionComponent* faction = registry.TryGet<FactionComponent>(candidate);
        if (faction == nullptr || faction->faction == selfFaction)
        {
            return false;
        }

        const HealthComponent* health = registry.TryGet<HealthComponent>(candidate);
        if (health == nullptr || health->current <= 0)
        {
            return false;
        }

        return registry.Has<GridPositionComponent>(candidate);
    }

    // Chebyshev distance to `other`, or a value past any range if it has no
    // position -- callers use this to decide "still in sight", so an
    // unlocatable entity has to read as out of range rather than adjacent.
    static int Distance(Registry& registry, int selfX, int selfY, Entity other)
    {
        const GridPositionComponent* position = registry.TryGet<GridPositionComponent>(other);
        if (position == nullptr)
        {
            return kUnreachable;
        }

        return Chebyshev(selfX, selfY, position->x, position->y);
    }

    static Entity FindNearest(Registry& registry, const TileGrid& tiles, Entity self, int selfFaction, int centerX,
                              int centerY, int range)
    {
        Entity best = kNullEntity;
        int bestDistance = range + 1;

        for (int y = centerY - range; y <= centerY + range; ++y)
        {
            for (int x = centerX - range; x <= centerX + range; ++x)
            {
                // Out-of-bounds tiles read as empty, so the scan needs no
                // clipping of its own.
                const Entity occupant = tiles.OccupantAt(x, y);
                if (occupant == kNullEntity || occupant == self)
                {
                    continue;
                }

                const int distance = Chebyshev(centerX, centerY, x, y);
                if (distance >= bestDistance)
                {
                    continue;
                }

                if (!IsEngageable(registry, selfFaction, occupant))
                {
                    continue;
                }

                bestDistance = distance;
                best = occupant;
            }
        }

        return best;
    }

    int DamageOf(Registry& registry, Entity attacker) const
    {
        const AttackPowerComponent* power = registry.TryGet<AttackPowerComponent>(attacker);
        return power != nullptr ? power->amount : defaultAttackDamage;
    }

    // Larger than any range an AIComponent can meaningfully carry.
    static constexpr int kUnreachable = 1 << 24;
};

} // namespace world_v2
