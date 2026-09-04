#include "../component/Combat.h"
#include "../component/Grid.h"
#include "../component/Request.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "../world/MapWorld.h"
#include "AISystem.h"

using namespace world_v2;

namespace
{

constexpr int kMonsters = 1;
constexpr int kPlayers = 2;

// A monster: sees, chases, and hits.
Entity SpawnHunter(MapWorld& world, int x, int y, int visionRange, int attackRange)
{
    const Entity entity = world.SpawnBlocking(x, y);
    world.registry.Assign<FactionComponent>(entity, kMonsters);
    world.registry.Assign<AIComponent>(entity, visionRange, attackRange, kNullEntity);
    world.registry.Assign<HealthComponent>(entity, 100, 100);
    return entity;
}

// Something for it to hunt.
Entity SpawnPrey(MapWorld& world, int x, int y, int faction = kPlayers)
{
    const Entity entity = world.SpawnBlocking(x, y);
    world.registry.Assign<FactionComponent>(entity, faction);
    world.registry.Assign<HealthComponent>(entity, 100, 100);
    return entity;
}

void NothingInSightMeansNoDecision()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 5, 5, 4, 1);

    ai.Update(world.registry, world.tiles);

    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, kNullEntity);
    CHECK(!world.registry.Has<MoveIntentComponent>(hunter));
    CHECK(!world.registry.Has<AttackRequestComponent>(hunter));
}

void OutOfReachMeansStepTowards()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 5, 5, 6, 1);
    const Entity prey = SpawnPrey(world, 9, 7);

    ai.Update(world.registry, world.tiles);

    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, prey);
    CHECK(world.registry.Has<MoveIntentComponent>(hunter));
    CHECK(!world.registry.Has<AttackRequestComponent>(hunter));

    // One tile of movement per axis, in the target's direction.
    const MoveIntentComponent& intent = world.registry.Get<MoveIntentComponent>(hunter);
    CHECK_EQ(intent.directionX, 1);
    CHECK_EQ(intent.directionY, 1);
}

void InReachMeansAttack()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 5, 5, 6, 1);
    const Entity prey = SpawnPrey(world, 6, 5);

    ai.Update(world.registry, world.tiles);

    CHECK(world.registry.Has<AttackRequestComponent>(hunter));
    CHECK(!world.registry.Has<MoveIntentComponent>(hunter));

    const AttackRequestComponent& request = world.registry.Get<AttackRequestComponent>(hunter);
    CHECK_EQ(request.targetEntity, prey);
    CHECK_EQ(request.damage, ai.defaultAttackDamage);
}

void DiagonalsCountAsOneTile()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    // Chebyshev distance 1, the same metric movement uses -- so a
    // diagonal neighbour is in reach of a range-1 attack.
    const Entity hunter = SpawnHunter(world, 5, 5, 6, 1);
    SpawnPrey(world, 6, 6);

    ai.Update(world.registry, world.tiles);

    CHECK(world.registry.Has<AttackRequestComponent>(hunter));
}

void AttackPowerOverridesTheDefault()
{
    MapWorld world(32, 32, true);
    AISystem ai;
    ai.defaultAttackDamage = 3;

    const Entity hunter = SpawnHunter(world, 5, 5, 6, 1);
    world.registry.Assign<AttackPowerComponent>(hunter, 17);
    SpawnPrey(world, 6, 5);

    ai.Update(world.registry, world.tiles);

    CHECK_EQ(world.registry.Get<AttackRequestComponent>(hunter).damage, 17);
}

void VisionRangeIsRespected()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 5, 5, 2, 1);
    SpawnPrey(world, 8, 5);

    ai.Update(world.registry, world.tiles);

    // Chebyshev distance 3, vision 2.
    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, kNullEntity);
    CHECK(!world.registry.Has<MoveIntentComponent>(hunter));
}

void OwnFactionIsNotATarget()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 5, 5, 6, 1);
    SpawnPrey(world, 6, 5, kMonsters);

    ai.Update(world.registry, world.tiles);

    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, kNullEntity);
    CHECK(!world.registry.Has<AttackRequestComponent>(hunter));
}

void UnfactionedEntitiesAreInvisible()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 5, 5, 6, 1);

    // No FactionComponent: nobody has said whose side it is on, so picking
    // a fight over it would be a guess.
    const Entity neutral = world.SpawnBlocking(6, 5);
    world.registry.Assign<HealthComponent>(neutral, 100, 100);

    ai.Update(world.registry, world.tiles);

    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, kNullEntity);
}

void NearestTargetWins()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 10, 10, 8, 1);

    // Spawned far-first, so picking the nearest cannot be an accident of
    // creation order.
    SpawnPrey(world, 10, 16);
    const Entity near = SpawnPrey(world, 10, 12);
    SpawnPrey(world, 4, 10);

    ai.Update(world.registry, world.tiles);

    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, near);
}

void TargetSelectionIsDeterministic()
{
    // Two entities equidistant in opposite directions: the tie has to
    // resolve the same way every run, or the server stops being
    // reproducible.
    Entity firstRun = kNullEntity;
    for (int run = 0; run < 4; ++run)
    {
        MapWorld world(32, 32, true);
        AISystem ai;

        const Entity hunter = SpawnHunter(world, 10, 10, 8, 1);
        SpawnPrey(world, 8, 10);
        SpawnPrey(world, 12, 10);

        ai.Update(world.registry, world.tiles);

        const Entity chosen = world.registry.Get<AIComponent>(hunter).currentTarget;
        CHECK(chosen != kNullEntity);

        if (run == 0)
        {
            firstRun = chosen;
        }
        else
        {
            CHECK_EQ(EntityIndex(chosen), EntityIndex(firstRun));
        }
    }
}

void TargetIsKeptWhileStillValid()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 10, 10, 8, 1);
    const Entity prey = SpawnPrey(world, 10, 14);

    ai.Update(world.registry, world.tiles);
    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, prey);

    // A second, closer target appearing does not make the hunter drop the
    // one it is already committed to.
    SpawnPrey(world, 10, 11);
    ai.Update(world.registry, world.tiles);
    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, prey);
}

void TargetIsDroppedWhenItLeavesVision()
{
    MapWorld world(64, 64, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 10, 10, 4, 1);
    const Entity prey = SpawnPrey(world, 10, 13);

    ai.Update(world.registry, world.tiles);
    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, prey);

    // Teleport it out of sight (moving through MapWorld would need the
    // movement system; the grid entry is what matters here).
    world.tiles.Remove(prey, 10, 13);
    world.registry.Get<GridPositionComponent>(prey).y = 40;
    world.tiles.TryPlace(prey, 10, 40);

    ai.Update(world.registry, world.tiles);
    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, kNullEntity);
}

void DeadEntitiesNeitherActNorAttract()
{
    MapWorld world(32, 32, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 5, 5, 6, 1);
    const Entity corpse = SpawnPrey(world, 6, 5);

    // A dead entity stays on the map until the barrier clears it, so every
    // system in between has to be able to tell it apart from a living one.
    world.registry.Get<HealthComponent>(corpse).current = 0;
    world.registry.Assign<DeadComponent>(corpse);

    ai.Update(world.registry, world.tiles);
    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, kNullEntity);
    CHECK(!world.registry.Has<AttackRequestComponent>(hunter));

    // And a dead hunter makes no decisions of its own.
    const Entity deadHunter = SpawnHunter(world, 20, 20, 6, 1);
    world.registry.Assign<DeadComponent>(deadHunter);
    SpawnPrey(world, 21, 20);

    ai.Update(world.registry, world.tiles);
    CHECK(!world.registry.Has<AttackRequestComponent>(deadHunter));
    CHECK(!world.registry.Has<MoveIntentComponent>(deadHunter));
}

void ScanCostDoesNotDependOnMapPopulation()
{
    MapWorld world(128, 128, true);
    AISystem ai;

    const Entity hunter = SpawnHunter(world, 64, 64, 3, 1);

    // A thousand entities well outside vision. The scan is bounded by
    // visionRange, so none of them are ever looked at -- this is the
    // property the occupancy grid exists for, and it fails loudly (a wrong
    // target) if the search ever degenerates into a world sweep.
    for (int i = 0; i < 1000; ++i)
    {
        SpawnPrey(world, i % 100, 100 + (i / 100));
    }

    ai.Update(world.registry, world.tiles);
    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, kNullEntity);

    const Entity close = SpawnPrey(world, 66, 64);
    ai.Update(world.registry, world.tiles);
    CHECK_EQ(world.registry.Get<AIComponent>(hunter).currentTarget, close);
}

} // namespace

int main()
{
    NothingInSightMeansNoDecision();
    OutOfReachMeansStepTowards();
    InReachMeansAttack();
    DiagonalsCountAsOneTile();
    AttackPowerOverridesTheDefault();
    VisionRangeIsRespected();
    OwnFactionIsNotATarget();
    UnfactionedEntitiesAreInvisible();
    NearestTargetWins();
    TargetSelectionIsDeterministic();
    TargetIsKeptWhileStillValid();
    TargetIsDroppedWhenItLeavesVision();
    DeadEntitiesNeitherActNorAttract();
    ScanCostDoesNotDependOnMapPopulation();

    return world_v2::test::Summary("AISystem");
}
