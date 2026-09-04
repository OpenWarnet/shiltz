#pragma once

#include "../core/Entity.h"

namespace world_v2
{

// An entity stepped from one tile to the next.
//
// Emitted the moment the step is committed, which is also the moment the
// server considers the entity to be on the target tile. Everything a client
// needs to animate the move is here -- both endpoints and how fast -- so the
// network layer never has to reconstruct it by diffing positions between
// ticks.
//
// Only actual moves are announced. A rejected intent -- a wall, another
// creature, a malformed direction -- produces nothing, because nothing
// happened.
struct EntityMovedEvent
{
    Entity entity = kNullEntity;
    int fromX = 0;
    int fromY = 0;
    int toX = 0;
    int toY = 0;

    // Tiles per second, so a client can pace the slide it draws.
    float speed = 0.0f;
};

} // namespace world_v2
