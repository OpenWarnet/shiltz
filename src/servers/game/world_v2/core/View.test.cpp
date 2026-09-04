#include "Entity.h"
#include "Registry.h"
#include "Test.h"
#include "View.h"

#include <algorithm>
#include <cstddef>
#include <vector>

using namespace world_v2;

namespace
{

struct GridPosition
{
    int x = 0;
    int y = 0;
};

struct MoveIntent
{
    int dx = 0;
    int dy = 0;
};

struct Health
{
    int current = 0;
};

std::vector<Entity> Collect(Registry& registry)
{
    std::vector<Entity> seen;
    registry.view<GridPosition, MoveIntent>().Each(
        [&seen](Entity entity, GridPosition&, MoveIntent&) { seen.push_back(entity); });

    std::sort(seen.begin(), seen.end());
    return seen;
}

void FiltersOnEveryComponent()
{
    Registry registry;

    const Entity both = registry.Create();
    registry.Assign<GridPosition>(both, 1, 1);
    registry.Assign<MoveIntent>(both, 1, 0);

    const Entity positionOnly = registry.Create();
    registry.Assign<GridPosition>(positionOnly, 2, 2);

    const Entity intentOnly = registry.Create();
    registry.Assign<MoveIntent>(intentOnly, 0, 1);

    const Entity neither = registry.Create();
    registry.Assign<Health>(neither, 10);

    const std::vector<Entity> seen = Collect(registry);
    CHECK_EQ(seen.size(), 1u);
    if (seen.size() == 1)
    {
        CHECK_EQ(seen[0], both);
    }

    View<GridPosition, MoveIntent> view = registry.view<GridPosition, MoveIntent>();
    CHECK(view.Contains(both));
    CHECK(!view.Contains(positionOnly));
    CHECK(!view.Contains(intentOnly));
}

void DrivesOffTheSmallestPool()
{
    Registry registry;

    // 200 entities can stand still; 4 of them want to move. The view must
    // consider 4 candidates, not 200 -- that is the whole reason for
    // picking the smallest pool rather than sweeping the world.
    for (int i = 0; i < 200; ++i)
    {
        const Entity entity = registry.Create();
        registry.Assign<GridPosition>(entity, i, i);

        if (i % 64 == 0)
        {
            registry.Assign<MoveIntent>(entity, 1, 0);
        }
    }

    CHECK_EQ(registry.Count<GridPosition>(), 200u);
    CHECK_EQ(registry.Count<MoveIntent>(), 4u);

    // Held in locals because a comma inside the template argument list
    // would otherwise be read as a second macro argument.
    const std::size_t candidates = registry.view<GridPosition, MoveIntent>().CandidateCount();
    CHECK_EQ(candidates, 4u);

    // Component order in the type list must not change which pool drives.
    const std::size_t reordered = registry.view<MoveIntent, GridPosition>().CandidateCount();
    CHECK_EQ(reordered, 4u);
}

void ReadsAndWritesThrough()
{
    Registry registry;

    const Entity entity = registry.Create();
    registry.Assign<GridPosition>(entity, 3, 4);
    registry.Assign<MoveIntent>(entity, 1, -1);

    registry.view<GridPosition, MoveIntent>().Each(
        [](Entity, GridPosition& position, MoveIntent& intent)
        {
            position.x += intent.dx;
            position.y += intent.dy;
        });

    CHECK_EQ(registry.Get<GridPosition>(entity).x, 4);
    CHECK_EQ(registry.Get<GridPosition>(entity).y, 3);
}

void RangeForMatchesEach()
{
    Registry registry;

    for (int i = 0; i < 10; ++i)
    {
        const Entity entity = registry.Create();
        registry.Assign<GridPosition>(entity, i, i);
        if (i % 2 == 0)
        {
            registry.Assign<MoveIntent>(entity, 1, 0);
        }
    }

    View<GridPosition, MoveIntent> view = registry.view<GridPosition, MoveIntent>();

    std::vector<Entity> byRangeFor;
    for (Entity entity : view)
    {
        CHECK_EQ(view.Get<MoveIntent>(entity).dx, 1);
        byRangeFor.push_back(entity);
    }
    std::sort(byRangeFor.begin(), byRangeFor.end());

    CHECK_EQ(byRangeFor.size(), 5u);
    CHECK(byRangeFor == Collect(registry));
}

void EmptyViews()
{
    Registry registry;

    // Nothing has ever held either component.
    std::size_t visits = 0;
    registry.view<GridPosition, MoveIntent>().Each(
        [&visits](Entity, GridPosition&, MoveIntent&) { ++visits; });
    CHECK_EQ(visits, 0u);

    for (Entity entity : registry.view<GridPosition, MoveIntent>())
    {
        (void)entity;
        ++visits;
    }
    CHECK_EQ(visits, 0u);

    // One populated pool, one empty one.
    const Entity entity = registry.Create();
    registry.Assign<GridPosition>(entity, 0, 0);

    registry.view<GridPosition, MoveIntent>().Each(
        [&visits](Entity, GridPosition&, MoveIntent&) { ++visits; });
    CHECK_EQ(visits, 0u);
}

void RemovingTheCurrentEntityIsSafe()
{
    Registry registry;

    std::vector<Entity> entities;
    for (int i = 0; i < 20; ++i)
    {
        const Entity entity = registry.Create();
        registry.Assign<GridPosition>(entity, i, i);
        registry.Assign<MoveIntent>(entity, 1, 0);
        entities.push_back(entity);
    }

    // Padding so that MoveIntent is strictly the smaller pool and therefore
    // the one driving the walk. Without this the two pools tie, GridPosition
    // wins the tie, and removing MoveIntent below would never touch the
    // array being iterated -- the test would pass without exercising
    // anything.
    for (int i = 0; i < 5; ++i)
    {
        registry.Assign<GridPosition>(registry.Create(), 0, 0);
    }

    const std::size_t driving = registry.view<GridPosition, MoveIntent>().CandidateCount();
    CHECK_EQ(driving, 20u);

    // The GridMovementSystem pattern: act on the intent, then consume it.
    // Removing from the driving pool reorders it underneath the walk, which
    // the backwards iteration is there to survive.
    std::vector<Entity> seen;
    registry.view<GridPosition, MoveIntent>().Each(
        [&](Entity entity, GridPosition& position, MoveIntent& intent)
        {
            position.x += intent.dx;
            seen.push_back(entity);
            registry.Remove<MoveIntent>(entity);
        });

    std::sort(seen.begin(), seen.end());
    std::vector<Entity> expected = entities;
    std::sort(expected.begin(), expected.end());

    // Every entity visited exactly once -- no skips, no doubles.
    CHECK_EQ(seen.size(), 20u);
    CHECK(seen == expected);
    CHECK_EQ(registry.Count<MoveIntent>(), 0u);
    CHECK_EQ(registry.Count<GridPosition>(), 25u);

    for (Entity entity : entities)
    {
        CHECK_EQ(registry.Get<GridPosition>(entity).x, registry.Get<GridPosition>(entity).y + 1);
    }
}

void DestroyingTheCurrentEntityIsSafe()
{
    Registry registry;

    for (int i = 0; i < 20; ++i)
    {
        const Entity entity = registry.Create();
        registry.Assign<GridPosition>(entity, i, i);
        registry.Assign<MoveIntent>(entity, 1, 0);
        registry.Assign<Health>(entity, 0);
    }

    // Destroy evicts from all three pools at once, so the driving array can
    // shrink by more than the one element the cursor is sitting on.
    std::size_t visits = 0;
    registry.view<GridPosition, MoveIntent>().Each(
        [&](Entity entity, GridPosition&, MoveIntent&)
        {
            ++visits;
            registry.Destroy(entity);
        });

    CHECK_EQ(visits, 20u);
    CHECK_EQ(registry.AliveCount(), 0u);
    CHECK_EQ(registry.Count<GridPosition>(), 0u);
    CHECK_EQ(registry.Count<MoveIntent>(), 0u);
    CHECK_EQ(registry.Count<Health>(), 0u);
}

void EntitiesAddedDuringIterationAreNotVisited()
{
    Registry registry;

    for (int i = 0; i < 5; ++i)
    {
        const Entity entity = registry.Create();
        registry.Assign<GridPosition>(entity, i, i);
        registry.Assign<MoveIntent>(entity, 1, 0);
    }

    // A system that spawns while iterating must not have its spawns swept
    // in the same pass -- a request created this tick belongs to the next
    // one.
    std::size_t visits = 0;
    registry.view<GridPosition, MoveIntent>().Each(
        [&](Entity, GridPosition&, MoveIntent&)
        {
            ++visits;
            if (visits == 1)
            {
                for (int i = 0; i < 5; ++i)
                {
                    const Entity spawned = registry.Create();
                    registry.Assign<GridPosition>(spawned, 0, 0);
                    registry.Assign<MoveIntent>(spawned, 0, 0);
                }
            }
        });

    CHECK_EQ(visits, 5u);
    CHECK_EQ(registry.Count<MoveIntent>(), 10u);

    // The next pass does see them.
    std::size_t secondPass = 0;
    registry.view<GridPosition, MoveIntent>().Each(
        [&secondPass](Entity, GridPosition&, MoveIntent&) { ++secondPass; });
    CHECK_EQ(secondPass, 10u);
}

void SingleComponentView()
{
    Registry registry;

    for (int i = 0; i < 8; ++i)
    {
        const Entity entity = registry.Create();
        registry.Assign<Health>(entity, i);
    }

    int total = 0;
    registry.view<Health>().Each([&total](Entity, Health& health) { total += health.current; });

    CHECK_EQ(total, 0 + 1 + 2 + 3 + 4 + 5 + 6 + 7);
}

} // namespace

int main()
{
    FiltersOnEveryComponent();
    DrivesOffTheSmallestPool();
    ReadsAndWritesThrough();
    RangeForMatchesEach();
    EmptyViews();
    RemovingTheCurrentEntityIsSafe();
    DestroyingTheCurrentEntityIsSafe();
    EntitiesAddedDuringIterationAreNotVisited();
    SingleComponentView();

    return world_v2::test::Summary("View");
}
