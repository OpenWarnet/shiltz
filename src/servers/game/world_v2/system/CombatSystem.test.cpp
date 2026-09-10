#include "../component/Combat.h"
#include "../component/Grid.h"
#include "../component/Request.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "../event/CombatEvents.h"
#include "../core/Map.h"
#include "CombatSystem.h"
#include "DeathSystem.h"

#include <vector>

using namespace world_v2;

namespace
{

Entity SpawnFighter(Map& world, int x, int y, int health)
{
    const Entity entity = world.Spawn(x, y);
    world.registry.Assign<HealthComponent>(entity, health, health);
    return entity;
}

void LandedHitReducesHealth()
{
    Map world(32, 32, true);
    CombatSystem combat;

    const Entity attacker = SpawnFighter(world, 5, 5, 100);
    const Entity target = SpawnFighter(world, 6, 5, 50);

    std::vector<DamageDealtEvent> hits;
    world.events.Listen<DamageDealtEvent>([&hits](const DamageDealtEvent& event) { hits.push_back(event); });

    world.registry.Assign<AttackRequestComponent>(attacker, target, 12);
    combat.Update(world.registry, world.events);

    CHECK_EQ(world.registry.Get<HealthComponent>(target).current, 38);

    // The request is spent -- surviving it would hit twice next tick.
    CHECK(!world.registry.Has<AttackRequestComponent>(attacker));

    // Credit is recorded on the victim, for whoever announces the death.
    CHECK(world.registry.Has<LastAttackerComponent>(target));
    CHECK_EQ(world.registry.Get<LastAttackerComponent>(target).attacker, attacker);

    // The event is queued, not dispatched -- combat runs in the simulation
    // stage, and the barrier has not happened yet.
    CHECK_EQ(hits.size(), 0u);
    world.events.Flush();

    CHECK_EQ(hits.size(), 1u);
    if (hits.size() == 1)
    {
        CHECK_EQ(hits[0].attacker, attacker);
        CHECK_EQ(hits[0].target, target);
        CHECK_EQ(hits[0].amount, 12);
        CHECK_EQ(hits[0].remainingHealth, 38);
    }
}

void OutOfReachIsRejectedButStillConsumed()
{
    Map world(32, 32, true);
    CombatSystem combat;

    // Reach defaults to melee for an attacker with no AIComponent, since
    // its real reach comes from equipment the framework knows nothing
    // about.
    const Entity attacker = SpawnFighter(world, 5, 5, 100);
    const Entity target = SpawnFighter(world, 9, 5, 50);

    world.registry.Assign<AttackRequestComponent>(attacker, target, 12);
    combat.Update(world.registry, world.events);

    CHECK_EQ(world.registry.Get<HealthComponent>(target).current, 50);
    CHECK(!world.registry.Has<AttackRequestComponent>(attacker));
    CHECK(!world.registry.Has<LastAttackerComponent>(target));
}

void AttackRangeComesFromTheAiComponent()
{
    Map world(32, 32, true);
    CombatSystem combat;

    const Entity attacker = SpawnFighter(world, 5, 5, 100);
    world.registry.Assign<AIComponent>(attacker, 10, 4, kNullEntity);
    const Entity target = SpawnFighter(world, 9, 5, 50);

    world.registry.Assign<AttackRequestComponent>(attacker, target, 12);
    combat.Update(world.registry, world.events);

    CHECK_EQ(world.registry.Get<HealthComponent>(target).current, 38);
}

void MalformedRequestsAreDropped()
{
    Map world(32, 32, true);
    CombatSystem combat;

    const Entity attacker = SpawnFighter(world, 5, 5, 100);
    const Entity target = SpawnFighter(world, 6, 5, 50);

    // Zero and negative damage would be a heal by accident.
    world.registry.Assign<AttackRequestComponent>(attacker, target, 0);
    combat.Update(world.registry, world.events);
    CHECK_EQ(world.registry.Get<HealthComponent>(target).current, 50);

    world.registry.Assign<AttackRequestComponent>(attacker, target, -25);
    combat.Update(world.registry, world.events);
    CHECK_EQ(world.registry.Get<HealthComponent>(target).current, 50);

    // Self-harm by way of a crafted or confused request.
    world.registry.Assign<AttackRequestComponent>(attacker, attacker, 10);
    combat.Update(world.registry, world.events);
    CHECK_EQ(world.registry.Get<HealthComponent>(attacker).current, 100);
}

void StaleTargetsAreRejected()
{
    Map world(32, 32, true);
    CombatSystem combat;

    const Entity attacker = SpawnFighter(world, 5, 5, 100);
    const Entity target = SpawnFighter(world, 6, 5, 50);

    world.registry.Assign<AttackRequestComponent>(attacker, target, 12);
    world.Despawn(target);

    // The freed slot is taken over by an unrelated entity standing in the
    // same place. The generation in the handle is what stops the queued
    // request from hitting it.
    const Entity bystander = SpawnFighter(world, 6, 5, 50);
    CHECK_EQ(EntityIndex(bystander), EntityIndex(target));

    combat.Update(world.registry, world.events);

    CHECK_EQ(world.registry.Get<HealthComponent>(bystander).current, 50);
    CHECK(!world.registry.Has<LastAttackerComponent>(bystander));
}

void OverkillClampsAtZero()
{
    Map world(32, 32, true);
    CombatSystem combat;
    DeathSystem death;

    const Entity attacker = SpawnFighter(world, 5, 5, 100);
    const Entity target = SpawnFighter(world, 6, 5, 10);

    world.registry.Assign<AttackRequestComponent>(attacker, target, 9999);
    combat.Update(world.registry, world.events);

    // Nothing downstream should have to cope with negative health.
    CHECK_EQ(world.registry.Get<HealthComponent>(target).current, 0);

    death.Update(world.registry, world.events);
    CHECK_EQ(world.registry.Get<HealthComponent>(target).current, 0);
}

void DeathIsAnnouncedOnceAndCredited()
{
    Map world(32, 32, true);
    CombatSystem combat;
    DeathSystem death;

    const Entity attacker = SpawnFighter(world, 5, 5, 100);
    const Entity target = SpawnFighter(world, 6, 5, 10);

    std::vector<DeathEvent> deaths;
    world.events.Listen<DeathEvent>([&deaths](const DeathEvent& event) { deaths.push_back(event); });

    world.registry.Assign<AttackRequestComponent>(attacker, target, 10);
    combat.Update(world.registry, world.events);
    death.Update(world.registry, world.events);
    world.events.Flush();

    CHECK_EQ(deaths.size(), 1u);
    if (deaths.size() == 1)
    {
        CHECK_EQ(deaths[0].entity, target);
        CHECK_EQ(deaths[0].killer, attacker);
    }

    // The corpse is still present -- despawning is structural and belongs
    // at the barrier, not in the middle of a sweep.
    CHECK(world.registry.Exists(target));
    CHECK(world.registry.Has<DeadComponent>(target));

    // And it must not be announced again on any later pass, or its loot
    // drops twice.
    death.Update(world.registry, world.events);
    death.Update(world.registry, world.events);
    world.events.Flush();
    CHECK_EQ(deaths.size(), 1u);
}

void DeathWithoutAnAttackerHasNoKiller()
{
    Map world(32, 32, true);
    DeathSystem death;

    const Entity entity = SpawnFighter(world, 5, 5, 10);

    std::vector<DeathEvent> deaths;
    world.events.Listen<DeathEvent>([&deaths](const DeathEvent& event) { deaths.push_back(event); });

    // Health can reach zero from anything -- poison, a script, a fall.
    // Nothing has to remember to announce it.
    world.registry.Get<HealthComponent>(entity).current = 0;

    death.Update(world.registry, world.events);
    world.events.Flush();

    CHECK_EQ(deaths.size(), 1u);
    if (deaths.size() == 1)
    {
        CHECK_EQ(deaths[0].entity, entity);
        CHECK_EQ(deaths[0].killer, kNullEntity);
    }
}

void AlreadyDeadTargetsTakeNoMoreDamage()
{
    Map world(32, 32, true);
    CombatSystem combat;
    DeathSystem death;

    const Entity first = SpawnFighter(world, 5, 5, 100);
    const Entity second = SpawnFighter(world, 7, 5, 100);
    const Entity target = SpawnFighter(world, 6, 5, 10);

    world.registry.Assign<AttackRequestComponent>(first, target, 10);
    combat.Update(world.registry, world.events);
    death.Update(world.registry, world.events);

    // A second attacker swinging at the corpse in a later tick must not
    // re-credit the kill to itself.
    world.registry.Assign<AttackRequestComponent>(second, target, 10);
    combat.Update(world.registry, world.events);

    CHECK_EQ(world.registry.Get<LastAttackerComponent>(target).attacker, first);
    CHECK_EQ(world.registry.Get<HealthComponent>(target).current, 0);
}

void ManySimultaneousAttacks()
{
    Map world(64, 64, true);
    CombatSystem combat;
    DeathSystem death;

    // A ring of attackers around one target, all resolving in one pass --
    // each removing its own request from the pool being iterated.
    const Entity target = SpawnFighter(world, 32, 32, 100);

    std::vector<Entity> attackers;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }

            const Entity attacker = SpawnFighter(world, 32 + dx, 32 + dy, 100);
            CHECK(attacker != kNullEntity);
            world.registry.Assign<AttackRequestComponent>(attacker, target, 5);
            attackers.push_back(attacker);
        }
    }

    CHECK_EQ(attackers.size(), 8u);

    int hits = 0;
    world.events.Listen<DamageDealtEvent>([&hits](const DamageDealtEvent&) { ++hits; });

    combat.Update(world.registry, world.events);
    death.Update(world.registry, world.events);
    world.events.Flush();

    CHECK_EQ(hits, 8);
    CHECK_EQ(world.registry.Get<HealthComponent>(target).current, 60);
    CHECK_EQ(world.registry.Count<AttackRequestComponent>(), 0u);
}

} // namespace

int main()
{
    LandedHitReducesHealth();
    OutOfReachIsRejectedButStillConsumed();
    AttackRangeComesFromTheAiComponent();
    MalformedRequestsAreDropped();
    StaleTargetsAreRejected();
    OverkillClampsAtZero();
    DeathIsAnnouncedOnceAndCredited();
    DeathWithoutAnAttackerHasNoKiller();
    AlreadyDeadTargetsTakeNoMoreDamage();
    ManySimultaneousAttacks();

    return world_v2::test::Summary("CombatSystem");
}
