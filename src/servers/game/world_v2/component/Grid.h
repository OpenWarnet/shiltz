#pragma once

// Discrete grid position and movement.
//
// Server state is always a whole tile. An entity is at (4, 7) or at (5, 7);
// it is never at (4.5, 7). Everything about a move -- whether it is legal,
// what it collides with, who can see it -- is decided on integers, which is
// what makes collision an array index instead of geometry.
//
// The smooth slide a player sees between those two tiles is the client's
// business. The server sends it the endpoints and a speed and lets it
// animate; nothing here stores a fractional position, because nothing here
// needs one.

namespace world_v2
{

// Where an entity is, in tile coordinates. The authoritative answer.
//
// Never write this directly -- an entity's tile is mirrored in Tile's
// occupancy index, and the two going out of sync means entities findable at
// tiles they left and invisible at the one they are on. Go through
// Map/GridMovementSystem, which update both together.
struct GridPositionComponent
{
    int x = 0;
    int y = 0;
};

// "I want to step one tile that way", from a player's input packet or an AI
// decision. A request, not an outcome: GridMovementSystem decides whether
// it happens.
//
// Consumed the tick it is looked at, accepted or rejected, so a stale intent
// can never keep pushing an entity. The one exception is an entity still
// finishing a previous step -- see InterpolatedMoveComponent.
//
// Each field is -1, 0, or +1. A larger value is not a longer step, it is a
// malformed request, and GridMovementSystem drops it rather than honoring
// it -- otherwise a crafted packet claiming directionX = 400 would be a
// teleport.
struct MoveIntentComponent
{
    int directionX = 0;
    int directionY = 0;
};

// Present only while an entity is between tiles.
//
// GridPositionComponent has *already* moved to the target when this appears
// -- the server commits the move up front and this records the transition
// so it can be described to clients (start tile, target tile, how fast) and
// so the entity's next step can be gated until it lands.
//
// That gate is what this is really for on a headless server. `progress`
// climbing to 1.0 is a move cooldown, not an animation: while the component
// exists the entity is mid-step and will not take another one. Clients do
// the actual interpolating from the endpoints they were sent.
struct InterpolatedMoveComponent
{
    int startX = 0;
    int startY = 0;
    int targetX = 0;
    int targetY = 0;

    // 0.0 at the moment of departure, removed once it reaches 1.0.
    float progress = 0.0f;

    // Tiles per second, so progress advances by speed * deltaSeconds.
    float speed = 0.0f;
};

} // namespace world_v2
