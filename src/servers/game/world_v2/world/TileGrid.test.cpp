#include "../core/Entity.h"
#include "../core/Test.h"
#include "TileGrid.h"

using namespace world_v2;

namespace
{

// Stand-in handles; TileGrid never interprets them beyond comparing.
constexpr Entity kA = 1;
constexpr Entity kB = 2;

TileGrid OpenField(int width, int height)
{
    return TileGrid(width, height, true);
}

void StartsBlockedUnlessToldOtherwise()
{
    // A grid nobody loaded terrain into must block everything, so a map
    // whose data failed to load leaves its occupants standing still rather
    // than walking through walls.
    const TileGrid unloaded(4, 4);
    CHECK(!unloaded.IsWalkable(0, 0));
    CHECK(!unloaded.IsWalkable(3, 3));

    const TileGrid open = OpenField(4, 4);
    CHECK(open.IsWalkable(0, 0));
    CHECK(open.IsWalkable(3, 3));
}

void Dimensions()
{
    const TileGrid grid = OpenField(8, 5);
    CHECK_EQ(grid.Width(), 8);
    CHECK_EQ(grid.Height(), 5);

    CHECK(grid.InBounds(0, 0));
    CHECK(grid.InBounds(7, 4));
    CHECK(!grid.InBounds(8, 4));
    CHECK(!grid.InBounds(7, 5));
    CHECK(!grid.InBounds(-1, 0));
    CHECK(!grid.InBounds(0, -1));
}

void OutOfBoundsIsInertNotFatal()
{
    TileGrid grid = OpenField(4, 4);

    // Map data and AI wander rolls both produce out-of-range coordinates.
    // Reads answer "blocked and empty"; writes are dropped.
    CHECK(!grid.IsWalkable(99, 99));
    CHECK(!grid.IsFree(99, 99));
    CHECK_EQ(grid.OccupantAt(99, 99), kNullEntity);
    CHECK_EQ(grid.OccupantAt(-1, -1), kNullEntity);

    grid.SetWalkable(99, 99, true);
    grid.Vacate(-5, -5);
    grid.Remove(kA, 99, 99);
    CHECK(!grid.TryPlace(kA, 99, 99));

    CHECK_EQ(grid.Width(), 4);
}

void WalkabilityIsSeparateFromOccupancy()
{
    TileGrid grid = OpenField(4, 4);

    grid.SetWalkable(1, 1, false);
    CHECK(!grid.IsWalkable(1, 1));
    CHECK(!grid.IsFree(1, 1));
    CHECK_EQ(grid.OccupantAt(1, 1), kNullEntity);

    // Standing on a tile does not make it unwalkable -- it makes it taken.
    CHECK(grid.TryPlace(kA, 2, 2));
    CHECK(grid.IsWalkable(2, 2));
    CHECK(!grid.IsFree(2, 2));
    CHECK_EQ(grid.OccupantAt(2, 2), kA);
}

void PlacementIsExclusive()
{
    TileGrid grid = OpenField(4, 4);

    CHECK(grid.TryPlace(kA, 1, 1));

    // Someone else's tile.
    CHECK(!grid.TryPlace(kB, 1, 1));
    CHECK_EQ(grid.OccupantAt(1, 1), kA);

    // Its own tile, again -- idempotent, not a failure.
    CHECK(grid.TryPlace(kA, 1, 1));
    CHECK_EQ(grid.OccupantAt(1, 1), kA);

    // Terrain still wins.
    grid.SetWalkable(3, 3, false);
    CHECK(!grid.TryPlace(kB, 3, 3));
}

void RemoveOnlyEvictsTheNamedOccupant()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.TryPlace(kA, 2, 2));

    // A passable entity despawning at the same coordinates must not evict
    // the creature actually standing there.
    grid.Remove(kB, 2, 2);
    CHECK_EQ(grid.OccupantAt(2, 2), kA);

    grid.Remove(kA, 2, 2);
    CHECK_EQ(grid.OccupantAt(2, 2), kNullEntity);
    CHECK(grid.IsFree(2, 2));

    // Vacate is the unconditional form.
    CHECK(grid.TryPlace(kA, 2, 2));
    grid.Vacate(2, 2);
    CHECK_EQ(grid.OccupantAt(2, 2), kNullEntity);
}

void MoveTransfersTheTile()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.TryPlace(kA, 1, 1));

    CHECK(grid.TryMove(kA, 1, 1, 2, 1));
    CHECK_EQ(grid.OccupantAt(1, 1), kNullEntity);
    CHECK_EQ(grid.OccupantAt(2, 1), kA);
    CHECK(grid.IsFree(1, 1));
}

void BlockedMoveChangesNothing()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.TryPlace(kA, 1, 1));
    CHECK(grid.TryPlace(kB, 2, 1));
    grid.SetWalkable(1, 2, false);

    // Occupied destination.
    CHECK(!grid.TryMove(kA, 1, 1, 2, 1));
    CHECK_EQ(grid.OccupantAt(1, 1), kA);
    CHECK_EQ(grid.OccupantAt(2, 1), kB);

    // Unwalkable destination.
    CHECK(!grid.TryMove(kA, 1, 1, 1, 2));
    CHECK_EQ(grid.OccupantAt(1, 1), kA);

    // Off the map.
    CHECK(!grid.TryMove(kA, 1, 1, -1, 1));
    CHECK_EQ(grid.OccupantAt(1, 1), kA);

    // A failed move must never have half-applied -- the entity keeps the
    // tile it was on, rather than vacating into nowhere.
    CHECK(!grid.IsFree(1, 1));
}

void MoveOntoOwnTile()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.TryPlace(kA, 1, 1));

    // Vacate-then-claim ordering has to leave the tile claimed, not empty.
    CHECK(grid.TryMove(kA, 1, 1, 1, 1));
    CHECK_EQ(grid.OccupantAt(1, 1), kA);
}

void MoveIntoATileJustVacated()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.TryPlace(kA, 1, 1));
    CHECK(grid.TryPlace(kB, 2, 1));

    // B leaves, A follows into the same tile in the same pass.
    CHECK(grid.TryMove(kB, 2, 1, 3, 1));
    CHECK(grid.TryMove(kA, 1, 1, 2, 1));

    CHECK_EQ(grid.OccupantAt(1, 1), kNullEntity);
    CHECK_EQ(grid.OccupantAt(2, 1), kA);
    CHECK_EQ(grid.OccupantAt(3, 1), kB);
}

void DegenerateSizes()
{
    // Not worth a crash: a zero-sized map is simply one nothing fits on.
    TileGrid empty(0, 0, true);
    CHECK_EQ(empty.Width(), 0);
    CHECK(!empty.InBounds(0, 0));
    CHECK(!empty.TryPlace(kA, 0, 0));

    TileGrid negative(-4, -4, true);
    CHECK_EQ(negative.Width(), 0);
    CHECK_EQ(negative.Height(), 0);
}

} // namespace

int main()
{
    StartsBlockedUnlessToldOtherwise();
    Dimensions();
    OutOfBoundsIsInertNotFatal();
    WalkabilityIsSeparateFromOccupancy();
    PlacementIsExclusive();
    RemoveOnlyEvictsTheNamedOccupant();
    MoveTransfersTheTile();
    BlockedMoveChangesNothing();
    MoveOntoOwnTile();
    MoveIntoATileJustVacated();
    DegenerateSizes();

    return world_v2::test::Summary("TileGrid");
}
