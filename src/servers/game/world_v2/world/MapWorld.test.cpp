#include "../component/Grid.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "MapWorld.h"

using namespace world_v2;

namespace
{

void SpawnBlockingClaimsTheTile()
{
    MapWorld world(8, 8, true);

    const Entity entity = world.SpawnBlocking(3, 4);
    CHECK(entity != kNullEntity);
    CHECK(world.registry.Exists(entity));

    // Position and occupancy are two copies of one fact, written together.
    CHECK_EQ(world.registry.Get<GridPositionComponent>(entity).x, 3);
    CHECK_EQ(world.registry.Get<GridPositionComponent>(entity).y, 4);
    CHECK_EQ(world.tiles.OccupantAt(3, 4), entity);
    CHECK(!world.tiles.IsFree(3, 4));
}

void SpawnBlockingRefusesAndLeaksNothing()
{
    MapWorld world(8, 8, true);
    world.tiles.SetWalkable(1, 1, false);

    // Unwalkable terrain.
    CHECK_EQ(world.SpawnBlocking(1, 1), kNullEntity);

    // Off the map.
    CHECK_EQ(world.SpawnBlocking(99, 99), kNullEntity);

    // Already taken.
    const Entity first = world.SpawnBlocking(2, 2);
    CHECK(first != kNullEntity);
    CHECK_EQ(world.SpawnBlocking(2, 2), kNullEntity);

    // A refused spawn must not have consumed an entity slot on the way out.
    CHECK_EQ(world.registry.AliveCount(), 1u);
    CHECK_EQ(world.tiles.OccupantAt(2, 2), first);
}

void SpawnPassableDoesNotBlock()
{
    MapWorld world(8, 8, true);

    const Entity item = world.SpawnPassable(5, 5);
    CHECK(item != kNullEntity);

    // It has a position but is not in the occupancy array, so a creature
    // can still walk onto the tile.
    CHECK_EQ(world.registry.Get<GridPositionComponent>(item).x, 5);
    CHECK_EQ(world.tiles.OccupantAt(5, 5), kNullEntity);
    CHECK(world.tiles.IsFree(5, 5));

    const Entity creature = world.SpawnBlocking(5, 5);
    CHECK(creature != kNullEntity);
    CHECK_EQ(world.tiles.OccupantAt(5, 5), creature);
}

void SpawnPassableIgnoresTerrain()
{
    MapWorld world(8, 8, true);
    world.tiles.SetWalkable(6, 6, false);

    // An item dropped against a wall is fine; only walking there is not.
    const Entity item = world.SpawnPassable(6, 6);
    CHECK(item != kNullEntity);

    // But off the map is still off the map.
    CHECK_EQ(world.SpawnPassable(-1, 0), kNullEntity);
    CHECK_EQ(world.registry.AliveCount(), 1u);
}

void DespawnReleasesTheTile()
{
    MapWorld world(8, 8, true);

    const Entity entity = world.SpawnBlocking(3, 3);
    world.Despawn(entity);

    CHECK(!world.registry.Exists(entity));
    CHECK_EQ(world.registry.AliveCount(), 0u);

    // The tile must come back, or the map grows a phantom wall where
    // something used to stand.
    CHECK_EQ(world.tiles.OccupantAt(3, 3), kNullEntity);
    CHECK(world.tiles.IsFree(3, 3));

    // And it is immediately usable again.
    const Entity next = world.SpawnBlocking(3, 3);
    CHECK(next != kNullEntity);
    CHECK(next != entity);
}

void DespawningAPassableEntityLeavesTheOccupantAlone()
{
    MapWorld world(8, 8, true);

    const Entity item = world.SpawnPassable(4, 4);
    const Entity creature = world.SpawnBlocking(4, 4);
    CHECK(creature != kNullEntity);

    world.Despawn(item);

    // The item never claimed the tile, so removing it must not evict the
    // creature sharing those coordinates.
    CHECK(world.registry.Exists(creature));
    CHECK_EQ(world.tiles.OccupantAt(4, 4), creature);
}

void DespawnIsIdempotent()
{
    MapWorld world(8, 8, true);

    const Entity entity = world.SpawnBlocking(1, 1);
    world.Despawn(entity);
    world.Despawn(entity);
    world.Despawn(kNullEntity);

    CHECK_EQ(world.registry.AliveCount(), 0u);

    // A repeated despawn must not have vacated a tile that someone else has
    // since taken over.
    const Entity next = world.SpawnBlocking(1, 1);
    world.Despawn(entity);
    CHECK_EQ(world.tiles.OccupantAt(1, 1), next);
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
    SpawnBlockingClaimsTheTile();
    SpawnBlockingRefusesAndLeaksNothing();
    SpawnPassableDoesNotBlock();
    SpawnPassableIgnoresTerrain();
    DespawnReleasesTheTile();
    DespawningAPassableEntityLeavesTheOccupantAlone();
    DespawnIsIdempotent();
    EventsAreCarriedPerMap();

    return world_v2::test::Summary("MapWorld");
}
