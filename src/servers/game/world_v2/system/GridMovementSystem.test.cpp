#include "../component/Grid.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "../event/MovementEvents.h"
#include "../world/MapWorld.h"
#include "GridMovementSystem.h"

#include <cmath>
#include <cstddef>
#include <vector>

using namespace world_v2;

namespace
{

bool Near(float actual, float expected)
{
    return std::fabs(actual - expected) < 0.0001f;
}

void PositionIs(MapWorld& world, Entity entity, int x, int y)
{
    const GridPositionComponent& position = world.registry.Get<GridPositionComponent>(entity);
    CHECK_EQ(position.x, x);
    CHECK_EQ(position.y, y);
}

void AcceptedStepMovesEverythingTogether()
{
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    const Entity entity = world.Spawn(3, 3);
    world.registry.Assign<MoveIntentComponent>(entity, 1, 0);

    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    // Grid position and occupancy commit in the same breath -- a step is
    // fully taken or not taken at all.
    PositionIs(world, entity, 4, 3);
    CHECK(world.tiles.OccupantsAt(3, 3).empty());
    CHECK(world.tiles.Contains(entity, 4, 3));

    // The intent is spent...
    CHECK(!world.registry.Has<MoveIntentComponent>(entity));

    // ...and the step is recorded for the client to animate from.
    CHECK(world.registry.Has<InterpolatedMoveComponent>(entity));
    const InterpolatedMoveComponent& step = world.registry.Get<InterpolatedMoveComponent>(entity);
    CHECK_EQ(step.startX, 3);
    CHECK_EQ(step.startY, 3);
    CHECK_EQ(step.targetX, 4);
    CHECK_EQ(step.targetY, 3);
    CHECK(Near(step.progress, 0.0f));
    CHECK(Near(step.speed, GridMovementSystem::kDefaultTilesPerSecond));
}

void TerrainBlocksTheStep()
{
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    const Entity entity = world.Spawn(3, 3);
    world.tiles.SetWalkable(4, 3, false);
    world.registry.Assign<MoveIntentComponent>(entity, 1, 0);

    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    PositionIs(world, entity, 3, 3);
    CHECK(world.tiles.Contains(entity, 3, 3));
    CHECK(!world.registry.Has<InterpolatedMoveComponent>(entity));

    // Rejected still means consumed -- otherwise a creature walking into a
    // wall would re-attempt the same blocked step every tick forever.
    CHECK(!world.registry.Has<MoveIntentComponent>(entity));
}

void CreaturesWalkThroughEachOther()
{
    // Entities do not block entities. Walking into someone is just standing
    // where they are, so the step is committed like any other -- position
    // updated, tile index updated, a move announced.
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    const Entity mover = world.Spawn(3, 3);
    const Entity resident = world.Spawn(4, 3);
    CHECK(resident != kNullEntity);

    world.registry.Assign<MoveIntentComponent>(mover, 1, 0);
    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    PositionIs(world, mover, 4, 3);
    CHECK(world.tiles.OccupantsAt(3, 3).empty());
    CHECK_EQ(world.tiles.OccupantCount(4, 3), std::size_t{2});
    CHECK(world.tiles.Contains(mover, 4, 3));
    CHECK(world.tiles.Contains(resident, 4, 3));

    // A committed step, so it is mid-move like any other.
    CHECK(world.registry.Has<InterpolatedMoveComponent>(mover));

    // And the resident was not disturbed by being walked onto.
    PositionIs(world, resident, 4, 3);
    CHECK(!world.registry.Has<InterpolatedMoveComponent>(resident));
}

void AWholeCrowdCanStandOnOneTile()
{
    // The stacking case at scale, driven through movement rather than
    // placement: eight creatures converging on one square all arrive.
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    const Entity centre = world.Spawn(4, 4);
    CHECK(centre != kNullEntity);

    std::vector<Entity> arrivals;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }

            const Entity entity = world.Spawn(4 + dx, 4 + dy);
            CHECK(entity != kNullEntity);
            world.registry.Assign<MoveIntentComponent>(entity, -dx, -dy);
            arrivals.push_back(entity);
        }
    }

    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    // All eight neighbours plus the one already there.
    CHECK_EQ(world.tiles.OccupantCount(4, 4), std::size_t{9});
    for (const Entity entity : arrivals)
    {
        PositionIs(world, entity, 4, 4);
        CHECK(world.tiles.Contains(entity, 4, 4));
    }
}

void NonAdjacentIntentIsRejected()
{
    MapWorld world(16, 16, true);
    GridMovementSystem movement;

    const Entity entity = world.Spawn(5, 5);

    // A crafted packet claiming a huge direction is a malformed request,
    // not a longer step. Honoring it would be a teleport.
    world.registry.Assign<MoveIntentComponent>(entity, 400, 0);
    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    PositionIs(world, entity, 5, 5);
    CHECK(!world.registry.Has<InterpolatedMoveComponent>(entity));
    CHECK(!world.registry.Has<MoveIntentComponent>(entity));

    world.registry.Assign<MoveIntentComponent>(entity, 0, -2);
    movement.Update(world.registry, world.tiles, world.events, 0.0f);
    PositionIs(world, entity, 5, 5);
    CHECK(!world.registry.Has<InterpolatedMoveComponent>(entity));
}

void ZeroIntentIsConsumedWithoutMoving()
{
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    const Entity entity = world.Spawn(3, 3);
    world.registry.Assign<MoveIntentComponent>(entity, 0, 0);

    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    PositionIs(world, entity, 3, 3);
    CHECK(!world.registry.Has<MoveIntentComponent>(entity));
    CHECK(!world.registry.Has<InterpolatedMoveComponent>(entity));
}

void DiagonalStep()
{
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    const Entity entity = world.Spawn(3, 3);
    world.registry.Assign<MoveIntentComponent>(entity, -1, 1);

    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    PositionIs(world, entity, 2, 4);
    CHECK(world.tiles.Contains(entity, 2, 4));
}

void StepRetiresWhenProgressCompletes()
{
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    const Entity entity = world.Spawn(3, 3);
    world.registry.Assign<MoveIntentComponent>(entity, 1, 0);

    // 4 tiles/sec at 0.1s advances 0.4 per tick, so the step takes three.
    movement.Update(world.registry, world.tiles, world.events, 0.1f);
    CHECK(Near(world.registry.Get<InterpolatedMoveComponent>(entity).progress, 0.4f));

    movement.Update(world.registry, world.tiles, world.events, 0.1f);
    CHECK(Near(world.registry.Get<InterpolatedMoveComponent>(entity).progress, 0.8f));

    movement.Update(world.registry, world.tiles, world.events, 0.1f);
    CHECK(!world.registry.Has<InterpolatedMoveComponent>(entity));
    CHECK_EQ(world.registry.Count<InterpolatedMoveComponent>(), 0u);

    // The position was never in doubt -- it committed on the first tick.
    PositionIs(world, entity, 4, 3);
}

void IntentIsHeldWhileMidStep()
{
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    const Entity entity = world.Spawn(3, 3);
    world.registry.Assign<MoveIntentComponent>(entity, 1, 0);

    movement.Update(world.registry, world.tiles, world.events, 0.1f);
    PositionIs(world, entity, 4, 3);

    // A second intent arrives while the first step is still in flight. It
    // must be kept, not consumed -- this is what lets a player hold a
    // direction down and keep moving.
    world.registry.Assign<MoveIntentComponent>(entity, 1, 0);

    movement.Update(world.registry, world.tiles, world.events, 0.1f);
    CHECK(world.registry.Has<MoveIntentComponent>(entity));
    PositionIs(world, entity, 4, 3);

    // Third tick pushes progress past 1.0 and retires the step, but only
    // after this tick's intent pass has already run.
    movement.Update(world.registry, world.tiles, world.events, 0.1f);
    CHECK(world.registry.Has<MoveIntentComponent>(entity));
    CHECK(!world.registry.Has<InterpolatedMoveComponent>(entity));
    PositionIs(world, entity, 4, 3);

    // Now the held intent is free to become the next step.
    movement.Update(world.registry, world.tiles, world.events, 0.1f);
    PositionIs(world, entity, 5, 3);
    CHECK(world.tiles.OccupantsAt(4, 3).empty());
    CHECK(world.tiles.Contains(entity, 5, 3));
    CHECK(!world.registry.Has<MoveIntentComponent>(entity));
}

void ManyMoversInOnePass()
{
    MapWorld world(64, 8, true);
    GridMovementSystem movement;

    // A column of movers all stepping right, spaced so none blocks another,
    // plus a crowd of bystanders that must be untouched. The bystanders
    // also make MoveIntent strictly the smaller pool, so it is the one
    // driving the view -- the case where consuming intents reorders the
    // array being walked.
    std::vector<Entity> movers;
    for (int i = 0; i < 8; ++i)
    {
        const Entity entity = world.Spawn(i * 4, 1);
        CHECK(entity != kNullEntity);
        world.registry.Assign<MoveIntentComponent>(entity, 1, 0);
        movers.push_back(entity);
    }

    std::vector<Entity> bystanders;
    for (int i = 0; i < 40; ++i)
    {
        const Entity entity = world.Spawn(i, 5);
        CHECK(entity != kNullEntity);
        bystanders.push_back(entity);
    }

    const std::size_t driving = world.registry.Count<MoveIntentComponent>();
    CHECK_EQ(driving, 8u);

    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    // Every mover stepped exactly once, and every intent was consumed.
    CHECK_EQ(world.registry.Count<MoveIntentComponent>(), 0u);
    CHECK_EQ(world.registry.Count<InterpolatedMoveComponent>(), 8u);

    for (int i = 0; i < 8; ++i)
    {
        PositionIs(world, movers[static_cast<std::size_t>(i)], i * 4 + 1, 1);
        CHECK(world.tiles.OccupantsAt(i * 4, 1).empty());
        CHECK(world.tiles.Contains(movers[static_cast<std::size_t>(i)], i * 4 + 1, 1));
    }

    for (int i = 0; i < 40; ++i)
    {
        PositionIs(world, bystanders[static_cast<std::size_t>(i)], i, 5);
        CHECK(world.tiles.Contains(bystanders[static_cast<std::size_t>(i)], i, 5));
    }
}

void CommittedStepsAreAnnounced()
{
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    std::vector<EntityMovedEvent> moves;
    world.events.Listen<EntityMovedEvent>([&moves](const EntityMovedEvent& event) { moves.push_back(event); });

    const Entity entity = world.Spawn(3, 3);
    world.registry.Assign<MoveIntentComponent>(entity, 1, 0);

    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    // Deferred like everything else: nothing is dispatched until the
    // barrier.
    CHECK_EQ(moves.size(), 0u);
    world.events.Flush();

    CHECK_EQ(moves.size(), 1u);
    if (moves.size() == 1)
    {
        CHECK_EQ(moves[0].entity, entity);
        CHECK_EQ(moves[0].fromX, 3);
        CHECK_EQ(moves[0].fromY, 3);
        CHECK_EQ(moves[0].toX, 4);
        CHECK_EQ(moves[0].toY, 3);
        CHECK(Near(moves[0].speed, GridMovementSystem::kDefaultTilesPerSecond));
    }
}

void RejectedStepsAnnounceNothing()
{
    MapWorld world(8, 8, true);
    GridMovementSystem movement;

    int announced = 0;
    world.events.Listen<EntityMovedEvent>([&announced](const EntityMovedEvent&) { ++announced; });

    const Entity blocked = world.Spawn(3, 3);
    world.tiles.SetWalkable(4, 3, false);
    world.registry.Assign<MoveIntentComponent>(blocked, 1, 0);

    const Entity malformed = world.Spawn(6, 6);
    world.registry.Assign<MoveIntentComponent>(malformed, 400, 0);

    movement.Update(world.registry, world.tiles, world.events, 0.0f);
    world.events.Flush();

    // Nothing happened, so there is nothing to announce -- a client must
    // not be told about a step the server refused.
    CHECK_EQ(announced, 0);
}

void MapEdgeBlocksTheStep()
{
    MapWorld world(4, 4, true);
    GridMovementSystem movement;

    const Entity entity = world.Spawn(0, 0);
    world.registry.Assign<MoveIntentComponent>(entity, -1, 0);

    movement.Update(world.registry, world.tiles, world.events, 0.0f);

    PositionIs(world, entity, 0, 0);
    CHECK(world.tiles.Contains(entity, 0, 0));
    CHECK(!world.registry.Has<InterpolatedMoveComponent>(entity));
}

} // namespace

int main()
{
    AcceptedStepMovesEverythingTogether();
    TerrainBlocksTheStep();
    CreaturesWalkThroughEachOther();
    AWholeCrowdCanStandOnOneTile();
    NonAdjacentIntentIsRejected();
    ZeroIntentIsConsumedWithoutMoving();
    DiagonalStep();
    StepRetiresWhenProgressCompletes();
    IntentIsHeldWhileMidStep();
    ManyMoversInOnePass();
    CommittedStepsAreAnnounced();
    RejectedStepsAnnounceNothing();
    MapEdgeBlocksTheStep();

    return world_v2::test::Summary("GridMovementSystem");
}
