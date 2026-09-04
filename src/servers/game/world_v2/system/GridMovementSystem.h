#pragma once

#include "../component/Grid.h"
#include "../core/Entity.h"
#include "../core/EventManager.h"
#include "../core/Registry.h"
#include "../event/MovementEvents.h"
#include "../world/TileGrid.h"

namespace world_v2
{

// Turns movement intents into committed tile changes, and retires steps
// once they finish.
//
// Runs in the simulation stage, so it makes no structural change it is not
// allowed to: the only components it removes belong to the entity a view is
// currently visiting, and the only one it adds lands past the cursor. See
// View.h for why that is safe.
//
// Two passes, in this order, because the second is what releases entities
// for the first.
class GridMovementSystem
{
public:
    // Tiles crossed per second. Uniform for now; when creatures need
    // individual speeds this becomes a component read here instead.
    static constexpr float kDefaultTilesPerSecond = 4.0f;

    float tilesPerSecond = kDefaultTilesPerSecond;

    void Update(Registry& registry, TileGrid& tiles, EventManager& events, float deltaSeconds)
    {
        ResolveIntents(registry, tiles, events);
        AdvanceSteps(registry, deltaSeconds);
    }

private:
    // Pass 1 -- decide each pending intent.
    //
    // The move is committed to GridPositionComponent and to the occupancy
    // index in the same breath, which is the whole point of routing it
    // through TileGrid::Move: a step is either fully taken or not taken at
    // all, never half.
    void ResolveIntents(Registry& registry, TileGrid& tiles, EventManager& events)
    {
        registry.view<GridPositionComponent, MoveIntentComponent>().Each(
            [&](Entity entity, GridPositionComponent& position, MoveIntentComponent& intent)
            {
                // Still finishing the last step. Keep the intent -- it
                // becomes the next step the moment this one lands, which
                // is what lets a player hold a direction down and move
                // continuously.
                if (registry.Has<InterpolatedMoveComponent>(entity))
                {
                    return;
                }

                // Read before consuming: removing the component below
                // leaves `intent` dangling.
                const int directionX = intent.directionX;
                const int directionY = intent.directionY;
                const int fromX = position.x;
                const int fromY = position.y;

                // One intent buys exactly one decision, accepted or not, so
                // a rejected step cannot keep re-firing every tick. Safe
                // here: this is the entity the view is currently visiting.
                // `position` is untouched by this -- different pool.
                registry.Remove<MoveIntentComponent>(entity);

                if (directionX == 0 && directionY == 0)
                {
                    return;
                }

                // A step is one tile. Anything else is a malformed request
                // rather than a longer move, and gets dropped -- this is
                // the server-authoritative check that stops a crafted
                // packet claiming directionX = 400 from being a teleport.
                if (directionX < -1 || directionX > 1 || directionY < -1 || directionY > 1)
                {
                    return;
                }

                const int toX = fromX + directionX;
                const int toY = fromY + directionY;

                // Terrain, and nothing else. Entities do not block each
                // other, so a step is refused only by a wall or the edge of
                // the map -- whoever is already standing on the destination
                // is not consulted, and several things sharing a tile is an
                // ordinary state rather than a collision to resolve.
                //
                // Still routed through TileGrid::Move rather than done here
                // for the same reason as before: leaving the old tile and
                // joining the new one must not half-apply.
                if (!tiles.Move(entity, fromX, fromY, toX, toY))
                {
                    return;
                }

                position.x = toX;
                position.y = toY;

                registry.Assign<InterpolatedMoveComponent>(
                    entity, fromX, fromY, toX, toY, 0.0f, tilesPerSecond);

                // Only committed steps are announced. A rejected intent
                // produces nothing, because nothing happened.
                events.Emit(EntityMovedEvent{entity, fromX, fromY, toX, toY, tilesPerSecond});
            });
    }

    // Pass 2 -- age the steps already in flight.
    //
    // On a headless server this is a cooldown, not an animation: the entity
    // is already standing on the target tile and has been since the step
    // was committed. What `progress` reaching 1.0 means is that it may take
    // another step. Clients were sent the endpoints and are doing the
    // actual sliding.
    void AdvanceSteps(Registry& registry, float deltaSeconds)
    {
        registry.view<InterpolatedMoveComponent>().Each(
            [&](Entity entity, InterpolatedMoveComponent& step)
            {
                step.progress += step.speed * deltaSeconds;

                if (step.progress >= 1.0f)
                {
                    // Current entity, single driving pool -- safe.
                    registry.Remove<InterpolatedMoveComponent>(entity);
                }
            });
    }
};

} // namespace world_v2
