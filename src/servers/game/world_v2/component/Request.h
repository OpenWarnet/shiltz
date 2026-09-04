#pragma once

#include "../core/Entity.h"

namespace world_v2
{

// One-tick request tags.
//
// Same shape as MoveIntentComponent: something asks, a system decides, and
// the tag is consumed whether or not it was honored. They exist as
// components rather than as a queue so that a view can filter them the same
// way it filters anything else -- "everyone who wants to attack this tick"
// is just a two-component view over a pool that is nearly always tiny.
//
// Every entity handle in here is an Entity rather than a raw id, so a
// request naming something that died earlier this tick fails
// Registry::Exists instead of hitting whoever inherited the slot.

struct AttackRequestComponent
{
    Entity targetEntity = kNullEntity;
    int damage = 0;
};

struct PickupItemRequestComponent
{
    Entity targetItemEntity = kNullEntity;
};

} // namespace world_v2
