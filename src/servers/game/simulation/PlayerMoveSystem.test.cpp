// The authority CG_MOVE gained by going through the simulation.
//
// The old handler wrote the client's claimed coordinates straight into the
// session and echoed them back, so the client decided where it was. These
// are the checks that stopped being vacuous once PlayerMoveSystem sat in
// between.

#include "simulation/PlayerMoveModule.h"
#include "world_v2/core/Map.h"
#include "world_v2/core/Test.h"

#include <cstddef>
#include <vector>

using namespace world_v2;
using namespace game_sim;

namespace
{

constexpr int kExtent = 512;

// Same arrangement GameSimulation builds: every tile walkable, because no
// collision data exists yet.
Entity PlacePlayer(Map& world, int x, int y, float speed)
{
    const Entity player = world.Spawn(x, y);
    world.registry.Assign<PlayerIdentityComponent>(player, std::uint32_t{4242}, std::uint32_t{0});
    world.registry.Assign<PlayerSpeedComponent>(player, speed);
    return player;
}

void Request(Map& world, Entity player, int x, int y, std::uint32_t direction = 0)
{
    world.registry.Assign<MoveRequestComponent>(player, x, y, direction, std::uint32_t{0});
}

// One tick of just this system, with the barrier, so listeners have run by
// the time the checks look.
void Step(Map& world, PlayerMoveSystem& system, float deltaSeconds)
{
    system.Update(world.registry, world.tiles, world.events, deltaSeconds);
    world.events.Flush();
}

void AShortMoveIsCommitted()
{
    Map world(kExtent, kExtent, true);
    PlayerMoveSystem system;

    const Entity player = PlacePlayer(world, 100, 100, 64.0f);

    std::vector<EntityMovedEvent> moves;
    world.events.Listen<EntityMovedEvent>([&moves](const EntityMovedEvent& event) { moves.push_back(event); });

    Request(world, player, 105, 100);
    Step(world, system, 0.1f);

    const GridPositionComponent& position = world.registry.Get<GridPositionComponent>(player);
    CHECK_EQ(position.x, 105);
    CHECK_EQ(position.y, 100);

    CHECK_EQ(moves.size(), std::size_t{1});
    CHECK_EQ(moves[0].toX, 105);

    // The tile index has to agree with the component, or the AI would see
    // this player at a tile it left.
    CHECK(world.tiles.Contains(player, 105, 100));
    CHECK(!world.tiles.Contains(player, 100, 100));
}

// The one that matters: a crafted packet naming a far-away coordinate used
// to be accepted and confirmed verbatim.
void ATeleportIsClampedNotAccepted()
{
    Map world(kExtent, kExtent, true);
    PlayerMoveSystem system;

    const Entity player = PlacePlayer(world, 10, 10, 64.0f);

    Request(world, player, 500, 10);
    Step(world, system, 0.1f);

    const GridPositionComponent& position = world.registry.Get<GridPositionComponent>(player);

    // 64 units/s * 0.1s * 3.0 slack = 19.2 units of travel allowed.
    CHECK(position.x > 10);
    CHECK(position.x <= 30);
    CHECK_EQ(position.y, 10);
    CHECK(world.tiles.Contains(player, position.x, position.y));
}

// Clamped rather than refused, so ordinary latency does not rubber-band.
void AClampedMoveStillMakesProgress()
{
    Map world(kExtent, kExtent, true);
    PlayerMoveSystem system;

    const Entity player = PlacePlayer(world, 10, 10, 64.0f);

    int previous = 10;
    for (int tick = 0; tick < 5; ++tick)
    {
        Request(world, player, 500, 10);
        Step(world, system, 0.1f);

        const int now = world.registry.Get<GridPositionComponent>(player).x;
        CHECK(now > previous);
        previous = now;
    }
}

void AnOutOfBoundsDestinationIsRefused()
{
    Map world(kExtent, kExtent, true);
    PlayerMoveSystem system;

    // Fast enough that the budget is not what stops it -- bounds are.
    const Entity player = PlacePlayer(world, 2, 2, 100000.0f);

    std::vector<EntityMovedEvent> moves;
    world.events.Listen<EntityMovedEvent>([&moves](const EntityMovedEvent& event) { moves.push_back(event); });

    Request(world, player, -50, 2);
    Step(world, system, 0.1f);

    const GridPositionComponent& position = world.registry.Get<GridPositionComponent>(player);
    CHECK_EQ(position.x, 2);
    CHECK_EQ(position.y, 2);

    // Still answered: the client is owed a reply to every CG_MOVE, and this
    // one snaps it back to where it actually is.
    CHECK_EQ(moves.size(), std::size_t{1});
    CHECK_EQ(moves[0].toX, 2);
    CHECK_EQ(moves[0].toY, 2);
}

// One request buys one decision, so a refused move cannot re-fire forever.
void ARequestIsConsumedWhetherOrNotItSucceeds()
{
    Map world(kExtent, kExtent, true);
    PlayerMoveSystem system;

    const Entity player = PlacePlayer(world, 100, 100, 64.0f);

    Request(world, player, 101, 100);
    Step(world, system, 0.1f);
    CHECK(!world.registry.Has<MoveRequestComponent>(player));

    Request(world, player, -50, 100);
    Step(world, system, 0.1f);
    CHECK(!world.registry.Has<MoveRequestComponent>(player));
}

// A tick with nothing pending says nothing -- no phantom acknowledgements.
void AQuietTickEmitsNothing()
{
    Map world(kExtent, kExtent, true);
    PlayerMoveSystem system;

    PlacePlayer(world, 100, 100, 64.0f);

    std::vector<EntityMovedEvent> moves;
    world.events.Listen<EntityMovedEvent>([&moves](const EntityMovedEvent& event) { moves.push_back(event); });

    Step(world, system, 0.1f);
    Step(world, system, 0.1f);

    CHECK_EQ(moves.size(), std::size_t{0});
}

// Turning on the spot is always legal, even when the step itself is not.
void FacingFollowsTheRequestEvenWhenTheStepIsRefused()
{
    Map world(kExtent, kExtent, true);
    PlayerMoveSystem system;

    const Entity player = PlacePlayer(world, 2, 2, 100000.0f);

    Request(world, player, -50, 2, /*direction=*/6);
    Step(world, system, 0.1f);

    CHECK_EQ(world.registry.Get<PlayerIdentityComponent>(player).facing, std::uint32_t{6});
    CHECK_EQ(world.registry.Get<GridPositionComponent>(player).x, 2);
}

// Terrain is not consulted today because no collision data exists -- but
// the check is already wired, so this is what starts passing the day map
// data lands.
void TerrainBlocksAStepOnceItExists()
{
    Map world(kExtent, kExtent, true);
    PlayerMoveSystem system;

    const Entity player = PlacePlayer(world, 100, 100, 64.0f);
    world.tiles.SetWalkable(101, 100, false);

    Request(world, player, 101, 100);
    Step(world, system, 0.1f);

    const GridPositionComponent& position = world.registry.Get<GridPositionComponent>(player);
    CHECK_EQ(position.x, 100);
    CHECK(world.tiles.Contains(player, 100, 100));
}

// A player with no speed component still moves, at the fallback rate,
// rather than being silently pinned by a missing stat.
void AMissingSpeedStatDoesNotPinThePlayer()
{
    Map world(kExtent, kExtent, true);
    PlayerMoveSystem system;

    const Entity player = world.Spawn(100, 100);
    world.registry.Assign<PlayerIdentityComponent>(player, std::uint32_t{1}, std::uint32_t{0});

    Request(world, player, 103, 100);
    Step(world, system, 0.1f);

    CHECK(world.registry.Get<GridPositionComponent>(player).x > 100);
}

} // namespace

int main()
{
    AShortMoveIsCommitted();
    ATeleportIsClampedNotAccepted();
    AClampedMoveStillMakesProgress();
    AnOutOfBoundsDestinationIsRefused();
    ARequestIsConsumedWhetherOrNotItSucceeds();
    AQuietTickEmitsNothing();
    FacingFollowsTheRequestEvenWhenTheStepIsRefused();
    TerrainBlocksAStepOnceItExists();
    AMissingSpeedStatDoesNotPinThePlayer();

    return world_v2::test::Summary("PlayerMoveSystem");
}
