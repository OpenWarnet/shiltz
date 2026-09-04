#include "../core/Entity.h"
#include "../core/Test.h"
#include "TileGrid.h"

#include <cstddef>

using namespace world_v2;

namespace
{

// Stand-in handles; TileGrid never interprets them beyond comparing.
constexpr Entity kA = 1;
constexpr Entity kB = 2;
constexpr Entity kC = 3;

TileGrid OpenField(int width, int height)
{
    return TileGrid(width, height, true);
}

// The single occupant of a tile, for the many assertions that expect
// exactly one. kNullEntity for an empty tile; deliberately fails rather
// than picking one when the tile is shared, so a test that meant "only A is
// here" cannot quietly pass while B is standing there too.
Entity Only(const TileGrid& grid, int x, int y)
{
    const std::vector<Entity>& occupants = grid.OccupantsAt(x, y);
    if (occupants.size() != 1)
    {
        return kNullEntity;
    }
    return occupants.front();
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
    CHECK(grid.OccupantsAt(99, 99).empty());
    CHECK(grid.OccupantsAt(-1, -1).empty());
    CHECK_EQ(grid.OccupantCount(99, 99), std::size_t{0});
    CHECK(!grid.Contains(kA, 99, 99));

    grid.SetWalkable(99, 99, true);
    grid.Vacate(-5, -5);
    grid.Remove(kA, 99, 99);
    CHECK(!grid.Place(kA, 99, 99));

    CHECK_EQ(grid.Width(), 4);
}

void WalkabilityIsSeparateFromOccupancy()
{
    TileGrid grid = OpenField(4, 4);

    grid.SetWalkable(1, 1, false);
    CHECK(!grid.IsWalkable(1, 1));
    CHECK(grid.OccupantsAt(1, 1).empty());

    // Standing on a tile does not make it unwalkable, and never did. What
    // changed is that it does not make it taken either -- terrain is now
    // the only thing IsWalkable has to report on, and the only thing that
    // can refuse a step.
    CHECK(grid.Place(kA, 2, 2));
    CHECK(grid.IsWalkable(2, 2));
    CHECK_EQ(Only(grid, 2, 2), kA);
}

void ManyEntitiesShareOneTile()
{
    // The core reversal. A tile is a place things stand, not a slot one
    // thing owns: a whole party on one square is an ordinary state, not a
    // collision to resolve.
    TileGrid grid = OpenField(4, 4);

    CHECK(grid.Place(kA, 1, 1));
    CHECK(grid.Place(kB, 1, 1));
    CHECK(grid.Place(kC, 1, 1));

    CHECK_EQ(grid.OccupantCount(1, 1), std::size_t{3});
    CHECK(grid.Contains(kA, 1, 1));
    CHECK(grid.Contains(kB, 1, 1));
    CHECK(grid.Contains(kC, 1, 1));

    // Arrival order is preserved, which is what lets AI targeting break
    // ties by scan order and still answer the same way twice.
    CHECK_EQ(grid.OccupantsAt(1, 1)[0], kA);
    CHECK_EQ(grid.OccupantsAt(1, 1)[1], kB);
    CHECK_EQ(grid.OccupantsAt(1, 1)[2], kC);
}

void PlacingTwiceDoesNotDuplicate()
{
    // Idempotent, so a caller that re-places an entity it never moved does
    // not leave it in the tile list twice -- which would make it visible to
    // two scans and removable only once.
    TileGrid grid = OpenField(4, 4);

    CHECK(grid.Place(kA, 1, 1));
    CHECK(grid.Place(kA, 1, 1));
    CHECK_EQ(grid.OccupantCount(1, 1), std::size_t{1});

    grid.Remove(kA, 1, 1);
    CHECK_EQ(grid.OccupantCount(1, 1), std::size_t{0});
}

void PlacementIgnoresTerrain()
{
    // Placement is authoring, not movement: loot lands where its corpse
    // fell, and a corpse can fall against a wall. Only walking onto a tile
    // consults the terrain.
    TileGrid grid = OpenField(4, 4);
    grid.SetWalkable(3, 3, false);

    CHECK(grid.Place(kB, 3, 3));
    CHECK_EQ(Only(grid, 3, 3), kB);
    CHECK(!grid.IsWalkable(3, 3));
}

void RemoveOnlyEvictsTheNamedOccupant()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.Place(kA, 2, 2));
    CHECK(grid.Place(kB, 2, 2));

    // Sharpened by shared tiles: an item despawning under a creature's feet
    // must take only itself off the tile.
    grid.Remove(kB, 2, 2);
    CHECK_EQ(Only(grid, 2, 2), kA);

    // A handle that was never there is a no-op, not an eviction.
    grid.Remove(kC, 2, 2);
    CHECK_EQ(Only(grid, 2, 2), kA);

    grid.Remove(kA, 2, 2);
    CHECK(grid.OccupantsAt(2, 2).empty());

    // Vacate is the unconditional form -- everyone, without asking.
    CHECK(grid.Place(kA, 2, 2));
    CHECK(grid.Place(kB, 2, 2));
    grid.Vacate(2, 2);
    CHECK(grid.OccupantsAt(2, 2).empty());
}

void MoveTransfersTheTile()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.Place(kA, 1, 1));

    CHECK(grid.Move(kA, 1, 1, 2, 1));
    CHECK(grid.OccupantsAt(1, 1).empty());
    CHECK_EQ(Only(grid, 2, 1), kA);
}

void MoveOntoAnOccupiedTileSucceeds()
{
    // What used to be the headline rejection. Walking into someone is now
    // simply standing where they are.
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.Place(kA, 1, 1));
    CHECK(grid.Place(kB, 2, 1));

    CHECK(grid.Move(kA, 1, 1, 2, 1));

    CHECK(grid.OccupantsAt(1, 1).empty());
    CHECK_EQ(grid.OccupantCount(2, 1), std::size_t{2});
    CHECK(grid.Contains(kA, 2, 1));
    CHECK(grid.Contains(kB, 2, 1));
}

void BlockedMoveChangesNothing()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.Place(kA, 1, 1));
    grid.SetWalkable(1, 2, false);

    // Unwalkable destination.
    CHECK(!grid.Move(kA, 1, 1, 1, 2));
    CHECK_EQ(Only(grid, 1, 1), kA);
    CHECK(grid.OccupantsAt(1, 2).empty());

    // Off the map.
    CHECK(!grid.Move(kA, 1, 1, -1, 1));
    CHECK_EQ(Only(grid, 1, 1), kA);

    // A failed move must never have half-applied -- the entity keeps the
    // tile it was on, rather than vacating into nowhere.
    CHECK(grid.Contains(kA, 1, 1));
}

void MoveOntoOwnTile()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.Place(kA, 1, 1));

    // Remove-then-place ordering has to leave the entity present, not gone.
    CHECK(grid.Move(kA, 1, 1, 1, 1));
    CHECK_EQ(Only(grid, 1, 1), kA);
}

void MoveIntoATileJustVacated()
{
    TileGrid grid = OpenField(4, 4);
    CHECK(grid.Place(kA, 1, 1));
    CHECK(grid.Place(kB, 2, 1));

    // B leaves, A follows into the same tile in the same pass.
    CHECK(grid.Move(kB, 2, 1, 3, 1));
    CHECK(grid.Move(kA, 1, 1, 2, 1));

    CHECK(grid.OccupantsAt(1, 1).empty());
    CHECK_EQ(Only(grid, 2, 1), kA);
    CHECK_EQ(Only(grid, 3, 1), kB);
}

void DegenerateSizes()
{
    // Not worth a crash: a zero-sized map is simply one nothing fits on.
    TileGrid empty(0, 0, true);
    CHECK_EQ(empty.Width(), 0);
    CHECK(!empty.InBounds(0, 0));
    CHECK(!empty.Place(kA, 0, 0));

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
    ManyEntitiesShareOneTile();
    PlacingTwiceDoesNotDuplicate();
    PlacementIgnoresTerrain();
    RemoveOnlyEvictsTheNamedOccupant();
    MoveTransfersTheTile();
    MoveOntoAnOccupiedTileSucceeds();
    BlockedMoveChangesNothing();
    MoveOntoOwnTile();
    MoveIntoATileJustVacated();
    DegenerateSizes();

    return world_v2::test::Summary("TileGrid");
}
