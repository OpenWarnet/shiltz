#include "../component/Grid.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "MapWorld.h"

#include <cstddef>

using namespace world_v2;

namespace
{

void SpawnPlacesTheEntityOnItsTile()
{
    MapWorld world(8, 8, true);

    const Entity entity = world.Spawn(3, 4);
    CHECK(entity != kNullEntity);
    CHECK(world.registry.Exists(entity));

    // Position and occupancy are two copies of one fact, written together.
    CHECK_EQ(world.registry.Get<GridPositionComponent>(entity).x, 3);
    CHECK_EQ(world.registry.Get<GridPositionComponent>(entity).y, 4);
    CHECK(world.tiles.Contains(entity, 3, 4));
    CHECK_EQ(world.tiles.OccupantCount(3, 4), std::size_t{1});
}

void SpawnRefusesOnlyOffTheMap()
{
    MapWorld world(8, 8, true);
    world.tiles.SetWalkable(1, 1, false);

    // Unwalkable terrain is not a refusal: placement is authoring, and loot
    // lands where its corpse fell. Only walking there is forbidden.
    const Entity againstAWall = world.Spawn(1, 1);
    CHECK(againstAWall != kNullEntity);

    // Off the map still is.
    CHECK_EQ(world.Spawn(99, 99), kNullEntity);
    CHECK_EQ(world.Spawn(-1, 0), kNullEntity);

    // A refused spawn must not have consumed an entity slot on the way out.
    CHECK_EQ(world.registry.AliveCount(), std::size_t{1});
}

void AnyNumberOfEntitiesShareATile()
{
    // What used to be two functions -- SpawnBlocking, which refused a taken
    // tile, and SpawnPassable, which stayed out of the index to avoid
    // blocking anyone -- is one, because neither behaviour has anything
    // left to mean.
    MapWorld world(8, 8, true);

    const Entity item = world.Spawn(5, 5);
    const Entity creature = world.Spawn(5, 5);
    const Entity another = world.Spawn(5, 5);

    CHECK(item != kNullEntity);
    CHECK(creature != kNullEntity);
    CHECK(another != kNullEntity);

    CHECK_EQ(world.tiles.OccupantCount(5, 5), std::size_t{3});
    CHECK(world.tiles.Contains(item, 5, 5));
    CHECK(world.tiles.Contains(creature, 5, 5));
    CHECK(world.tiles.Contains(another, 5, 5));

    // All three agree with the index about where they are.
    CHECK_EQ(world.registry.Get<GridPositionComponent>(item).x, 5);
    CHECK_EQ(world.registry.Get<GridPositionComponent>(creature).y, 5);
}

void DespawnReleasesTheTile()
{
    MapWorld world(8, 8, true);

    const Entity entity = world.Spawn(3, 3);
    world.Despawn(entity);

    CHECK(!world.registry.Exists(entity));
    CHECK_EQ(world.registry.AliveCount(), std::size_t{0});

    // The tile must come back, or the map grows a phantom the AI can still
    // see at a square nothing is standing on.
    CHECK(world.tiles.OccupantsAt(3, 3).empty());

    const Entity next = world.Spawn(3, 3);
    CHECK(next != kNullEntity);
    CHECK(next != entity);
}

void DespawningOneOccupantLeavesTheRestAlone()
{
    MapWorld world(8, 8, true);

    const Entity item = world.Spawn(4, 4);
    const Entity creature = world.Spawn(4, 4);
    CHECK(creature != kNullEntity);

    world.Despawn(item);

    // Removing one thing from a shared tile must take only that thing.
    CHECK(world.registry.Exists(creature));
    CHECK_EQ(world.tiles.OccupantCount(4, 4), std::size_t{1});
    CHECK(world.tiles.Contains(creature, 4, 4));
}

void DespawnIsIdempotent()
{
    MapWorld world(8, 8, true);

    const Entity entity = world.Spawn(1, 1);
    world.Despawn(entity);
    world.Despawn(entity);
    world.Despawn(kNullEntity);

    CHECK_EQ(world.registry.AliveCount(), std::size_t{0});

    // A repeated despawn must not have taken someone else off the tile it
    // used to hold.
    const Entity next = world.Spawn(1, 1);
    world.Despawn(entity);
    CHECK(world.tiles.Contains(next, 1, 1));
    CHECK_EQ(world.tiles.OccupantCount(1, 1), std::size_t{1});
}

void EventsAreCarriedPerMap()
{
    MapWorld world(4, 4, true);

    // The bundle owns its own queue, so one map's barrier never dispatches
    // another map's events.
    struct Spawned
    {
        int value = 0;
    };

    int seen = 0;
    world.events.Listen<Spawned>([&seen](const Spawned& event) { seen += event.value; });

    world.events.Emit(Spawned{5});
    CHECK_EQ(seen, 0);

    world.events.Flush();
    CHECK_EQ(seen, 5);
}

} // namespace

int main()
{
    SpawnPlacesTheEntityOnItsTile();
    SpawnRefusesOnlyOffTheMap();
    AnyNumberOfEntitiesShareATile();
    DespawnReleasesTheTile();
    DespawningOneOccupantLeavesTheRestAlone();
    DespawnIsIdempotent();
    EventsAreCarriedPerMap();

    return world_v2::test::Summary("MapWorld");
}
