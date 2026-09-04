#include "Entity.h"
#include "Registry.h"
#include "Test.h"

#include <vector>

using namespace world_v2;

namespace
{

struct Health
{
    int current = 0;
    int max = 0;
};

struct GridPosition
{
    int x = 0;
    int y = 0;
};

struct Tag
{
    int value = 0;
};

void HandleBitPacking()
{
    const Entity entity = MakeEntity(1234, 7);
    CHECK_EQ(EntityIndex(entity), 1234u);
    CHECK_EQ(EntityGeneration(entity), 7u);

    // kNullEntity must not collide with any handle Create can produce --
    // its index is one past the largest allocatable one.
    CHECK_EQ(EntityIndex(kNullEntity), kEntityIndexMask);
    CHECK(kMaxEntityIndex < kEntityIndexMask);

    Registry registry;
    CHECK(!registry.Exists(kNullEntity));
}

void CreateAndDestroy()
{
    Registry registry;
    CHECK_EQ(registry.AliveCount(), 0u);

    const Entity a = registry.Create();
    const Entity b = registry.Create();

    CHECK(a != b);
    CHECK(registry.Exists(a));
    CHECK(registry.Exists(b));
    CHECK_EQ(registry.AliveCount(), 2u);

    registry.Destroy(a);
    CHECK(!registry.Exists(a));
    CHECK(registry.Exists(b));
    CHECK_EQ(registry.AliveCount(), 1u);

    // Destroying an already-dead handle is a no-op, not a double-free of
    // the slot -- otherwise the free list would hand the same index out
    // twice.
    registry.Destroy(a);
    CHECK_EQ(registry.AliveCount(), 1u);
}

void RecycledSlotRetiresOldHandle()
{
    Registry registry;

    const Entity first = registry.Create();
    registry.Destroy(first);

    const Entity second = registry.Create();

    // The slot comes back...
    CHECK_EQ(EntityIndex(second), EntityIndex(first));
    // ...but as a different handle, so last tick's copy of `first` cannot
    // be mistaken for it.
    CHECK(second != first);
    CHECK_EQ(EntityGeneration(second), EntityGeneration(first) + 1);
    CHECK(registry.Exists(second));
    CHECK(!registry.Exists(first));
}

void RecycledSlotDoesNotInheritComponents()
{
    Registry registry;

    const Entity first = registry.Create();
    registry.Assign<Health>(first, 50, 100);
    registry.Assign<Tag>(first, 1);
    CHECK_EQ(registry.Count<Health>(), 1u);

    registry.Destroy(first);

    // Destroy has to evict from *every* pool, not just the one the caller
    // happened to think about.
    CHECK_EQ(registry.Count<Health>(), 0u);
    CHECK_EQ(registry.Count<Tag>(), 0u);
    CHECK(!registry.Has<Health>(first));

    const Entity second = registry.Create();
    CHECK(!registry.Has<Health>(second));
    CHECK(!registry.Has<Tag>(second));
    CHECK(registry.TryGet<Health>(second) == nullptr);
}

void ComponentLifecycle()
{
    Registry registry;
    const Entity entity = registry.Create();

    CHECK(!registry.Has<Health>(entity));

    Health& health = registry.Assign<Health>(entity, 30, 40);
    CHECK(registry.Has<Health>(entity));
    CHECK_EQ(health.current, 30);
    CHECK_EQ(health.max, 40);

    // Assign returns a live reference into the pool.
    health.current = 12;
    CHECK_EQ(registry.Get<Health>(entity).current, 12);

    registry.Assign<GridPosition>(entity, 5, 6);
    CHECK_EQ(registry.Get<GridPosition>(entity).x, 5);
    CHECK_EQ(registry.Count<GridPosition>(), 1u);

    // Re-assigning replaces rather than duplicating.
    registry.Assign<Health>(entity, 99, 99);
    CHECK_EQ(registry.Count<Health>(), 1u);
    CHECK_EQ(registry.Get<Health>(entity).current, 99);

    registry.Remove<Health>(entity);
    CHECK(!registry.Has<Health>(entity));
    CHECK(registry.Has<GridPosition>(entity));

    // Removing something the entity does not have is a no-op.
    registry.Remove<Health>(entity);
    CHECK_EQ(registry.Count<Health>(), 0u);
}

void UnusedComponentTypesCostNothing()
{
    Registry registry;
    const Entity entity = registry.Create();

    // Querying a component type nothing has ever been assigned must answer
    // false without materializing a pool for it.
    CHECK(!registry.Has<Tag>(entity));
    CHECK_EQ(registry.Count<Tag>(), 0u);
    CHECK(registry.TryGet<Tag>(entity) == nullptr);
}

void TryGetOnStaleHandle()
{
    Registry registry;

    const Entity first = registry.Create();
    registry.Assign<Health>(first, 10, 10);
    registry.Destroy(first);

    const Entity second = registry.Create();
    registry.Assign<Health>(second, 77, 77);

    // The stale handle must not read the new occupant's health, even
    // though both name the same slot.
    CHECK(registry.TryGet<Health>(first) == nullptr);
    CHECK(registry.TryGet<Health>(second) != nullptr);
    CHECK_EQ(registry.TryGet<Health>(second)->current, 77);
}

void ManyEntitiesRoundTrip()
{
    Registry registry;

    std::vector<Entity> entities;
    for (int i = 0; i < 500; ++i)
    {
        const Entity entity = registry.Create();
        registry.Assign<GridPosition>(entity, i, i * 2);
        entities.push_back(entity);
    }

    CHECK_EQ(registry.AliveCount(), 500u);
    CHECK_EQ(registry.Count<GridPosition>(), 500u);

    // Destroy every other one, then confirm the survivors still resolve --
    // 250 swap-and-pops worth of sparse fix-ups.
    for (std::size_t i = 0; i < entities.size(); i += 2)
    {
        registry.Destroy(entities[i]);
    }

    CHECK_EQ(registry.AliveCount(), 250u);
    CHECK_EQ(registry.Count<GridPosition>(), 250u);

    for (std::size_t i = 1; i < entities.size(); i += 2)
    {
        CHECK(registry.Exists(entities[i]));
        CHECK_EQ(registry.Get<GridPosition>(entities[i]).x, static_cast<int>(i));
    }
}

} // namespace

int main()
{
    HandleBitPacking();
    CreateAndDestroy();
    RecycledSlotRetiresOldHandle();
    RecycledSlotDoesNotInheritComponents();
    ComponentLifecycle();
    UnusedComponentTypesCostNothing();
    TryGetOnStaleHandle();
    ManyEntitiesRoundTrip();

    return world_v2::test::Summary("Registry");
}
