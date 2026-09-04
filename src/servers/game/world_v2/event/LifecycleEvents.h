#pragma once

#include "../core/Entity.h"

namespace world_v2
{

// An entity's despawn timer ran out.
//
// Emitted by DespawnSystem in the simulation stage and resolved at the
// barrier, for the same reason DeathEvent is: removing an entity is
// structural, and a system sweeping a pool may not do it.
//
// Distinct from DeathEvent on purpose, even though both end in a despawn.
// A death has a killer, credits experience, and drops loot; an expiry has
// none of those and must not trigger them. Loot that times out on the floor
// should not pay somebody experience for it.
//
// Carries its own coordinates, like every other event a broadcast is built
// from: the entity is gone by the time a late listener looks, so one that
// had to ask the registry where it was would find nothing.
struct EntityExpiredEvent
{
    Entity entity = kNullEntity;
    int x = 0;
    int y = 0;
};

} // namespace world_v2
