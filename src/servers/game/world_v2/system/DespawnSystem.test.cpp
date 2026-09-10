// Despawn timers: the countdown, and what it is deliberately not.
//
// The interesting cases are all about the gap between announcing an expiry
// and resolving it. That gap is a whole tick stage wide -- the system emits
// in stage 2, the barrier removes in stage 3 -- and everything that can go
// wrong lives in it: announcing twice, colliding with a death, colliding
// with a pickup, or paying a killer for something nobody killed.

#include "../Simulation.h"
#include "BroadcastModule.h"
#include "CoreSimulationModule.h"
#include "../component/Combat.h"
#include "../component/Despawn.h"
#include "../component/Grid.h"
#include "../component/Items.h"
#include "../component/Network.h"
#include "../component/Request.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "../event/CombatEvents.h"
#include "../event/ItemEvents.h"
#include "../event/LifecycleEvents.h"
#include "../world/Inventory.h"
#include "CombatRules.h"
#include "DespawnRules.h"
#include "ItemRules.h"

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace world_v2;

namespace
{

constexpr std::uint32_t kPotion = 100;
constexpr float kStep = 0.25f;

void TheTimerCountsDownAndRemoves()
{
    Simulation simulation(32, 32, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<DespawnRulesModule>();

    std::vector<EntityExpiredEvent> expired;
    simulation.World().events.Listen<EntityExpiredEvent>(
        [&expired](const EntityExpiredEvent& event) { expired.push_back(event); });

    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 1, 10, 11, 10);
    CHECK(item != kNullEntity);
    simulation.World().registry.Assign<DespawnTimerComponent>(item, 1.0f);

    // Three quarter-second ticks leave a quarter on the clock.
    for (int i = 0; i < 3; ++i)
    {
        simulation.Tick(kStep);
        CHECK(simulation.World().registry.Exists(item));
        CHECK_EQ(expired.size(), std::size_t{0});
    }

    simulation.Tick(kStep);

    CHECK_EQ(expired.size(), std::size_t{1});
    if (expired.size() == 1)
    {
        CHECK_EQ(expired[0].entity, item);

        // The coordinates travel with the event, because by the time a
        // listener runs the entity may be gone.
        CHECK_EQ(expired[0].x, 11);
        CHECK_EQ(expired[0].y, 10);
    }

    // Removed at the barrier, in the same tick it was announced.
    CHECK(!simulation.World().registry.Exists(item));
    CHECK(simulation.World().tiles.OccupantsAt(11, 10).empty());
}

void ZeroMeansNextTickNotAlreadyExpired()
{
    // The countdown is applied before the test, so there is no way to build
    // a timer that never fires.
    Simulation simulation(32, 32, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<DespawnRulesModule>();

    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 1, 10, 5, 5);
    simulation.World().registry.Assign<DespawnTimerComponent>(item, 0.0f);

    CHECK(simulation.World().registry.Exists(item));
    simulation.Tick(kStep);
    CHECK(!simulation.World().registry.Exists(item));
}

void AnExpiryIsAnnouncedOnlyOnce()
{
    // The gap between announcing and resolving is a whole stage wide. A
    // game rule that keeps the entity alive -- a corpse expiring into bones
    // rather than vanishing -- leaves it sitting there with a spent timer,
    // and nothing may announce it a second time.
    Simulation simulation(32, 32, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();

    std::size_t announced = 0;
    simulation.World().events.Listen<EntityExpiredEvent>([&announced](const EntityExpiredEvent&) { ++announced; });

    // Deliberately no InstallDespawnRules: nothing removes the entity.
    const Entity lingering = simulation.World().Spawn(5, 5);
    simulation.World().registry.Assign<DespawnTimerComponent>(lingering, 0.1f);

    for (int i = 0; i < 10; ++i)
    {
        simulation.Tick(kStep);
    }

    CHECK(simulation.World().registry.Exists(lingering));
    CHECK_EQ(announced, std::size_t{1});
    CHECK(simulation.World().registry.Has<ExpiredComponent>(lingering));

    // And the clock stopped at zero rather than running away negative.
    CHECK(simulation.World().registry.Get<DespawnTimerComponent>(lingering).remaining <= 0.0f);
    CHECK(simulation.World().registry.Get<DespawnTimerComponent>(lingering).remaining >= 0.0f);
}

void APickupOnTheExpiryTickIsNotADoubleRemove()
{
    // The ordinary collision, not a rare one: pickup resolves in stage 2
    // before the despawn sweep, so an item taken on the very tick its timer
    // runs out is despawned by ItemRules and then named again by an expiry
    // that was emitted for it. The Exists guard in InstallDespawnRules is
    // what makes the second one a no-op.
    Simulation simulation(32, 32, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<ItemRulesModule>();
    simulation.Install<DespawnRulesModule>();

    std::vector<ItemPickedUpEvent> taken;
    simulation.World().events.Listen<ItemPickedUpEvent>([&taken](const ItemPickedUpEvent& e) { taken.push_back(e); });

    const Entity carrier = simulation.World().Spawn(10, 10);
    simulation.World().registry.Assign<InventoryComponent>(carrier, MakeInventory(4));

    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 3, 10, 11, 10);
    simulation.World().registry.Assign<DespawnTimerComponent>(item, kStep);
    simulation.World().registry.Assign<PickupItemRequestComponent>(carrier, item);

    simulation.Tick(kStep);

    // The pickup won: the goods are in the bag, exactly once.
    CHECK_EQ(taken.size(), std::size_t{1});
    CHECK_EQ(CountItem(simulation.World().registry.Get<InventoryComponent>(carrier), kPotion), std::uint32_t{3});
    CHECK(!simulation.World().registry.Exists(item));
}

void AnExpiryPaysNobodyAndDropsNothing()
{
    // The reason EntityExpiredEvent is not DeathEvent. Loot timing out on
    // the floor must not credit experience or roll a drop table -- if it
    // did, leaving a monster alone long enough would pay for killing it.
    Simulation simulation(32, 32, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<CombatRulesModule>();
    simulation.Install<DespawnRulesModule>();

    std::size_t deaths = 0;
    std::size_t drops = 0;
    std::size_t awards = 0;
    simulation.World().events.Listen<DeathEvent>([&deaths](const DeathEvent&) { ++deaths; });
    simulation.World().events.Listen<LootDropEvent>([&drops](const LootDropEvent&) { ++drops; });
    simulation.World().events.Listen<ExperienceAwardEvent>([&awards](const ExperienceAwardEvent&) { ++awards; });

    // Dressed exactly like something worth killing, then left to time out.
    const Entity monster = simulation.World().Spawn(5, 5);
    simulation.World().registry.Assign<HealthComponent>(monster, 50, 50);
    simulation.World().registry.Assign<ExperienceRewardComponent>(monster, std::uint64_t{500});
    simulation.World().registry.Assign<LootTableComponent>(monster, std::uint32_t{77});
    simulation.World().registry.Assign<LastAttackerComponent>(monster, simulation.World().Spawn(6, 5));
    simulation.World().registry.Assign<DespawnTimerComponent>(monster, kStep);

    simulation.Tick(kStep);

    CHECK(!simulation.World().registry.Exists(monster));
    CHECK_EQ(deaths, std::size_t{0});
    CHECK_EQ(drops, std::size_t{0});
    CHECK_EQ(awards, std::size_t{0});
}

void ADeathOnTheSameTickWins()
{
    // Despawn runs after death in stage 2, so something killed on the tick
    // its timer expires is reported as a death -- with its killer credited
    // and its loot rolled -- rather than as a silent expiry.
    Simulation simulation(32, 32, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<CombatRulesModule>();
    simulation.Install<DespawnRulesModule>();

    std::size_t deaths = 0;
    std::size_t expiries = 0;
    simulation.World().events.Listen<DeathEvent>([&deaths](const DeathEvent&) { ++deaths; });
    simulation.World().events.Listen<EntityExpiredEvent>([&expiries](const EntityExpiredEvent&) { ++expiries; });

    const Entity monster = simulation.World().Spawn(5, 5);
    simulation.World().registry.Assign<HealthComponent>(monster, 0, 50);
    simulation.World().registry.Assign<DespawnTimerComponent>(monster, kStep);

    simulation.Tick(kStep);

    CHECK(!simulation.World().registry.Exists(monster));
    CHECK_EQ(deaths, std::size_t{1});

    // The expiry still fires -- both clocks genuinely ran out -- but it
    // arrives at a barrier where the corpse is already gone, so it removes
    // nothing and pays nothing.
    CHECK_EQ(expiries, std::size_t{1});
}

void ExpiryIsBroadcastToViewersInRange()
{
    // A client told to draw something needs a way to be told to stop. The
    // Despawned notice is what pairs with Spawned.
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<DespawnRulesModule>();

    std::vector<Notice> seen;
    simulation.OnNotice([&seen](Entity, const Notice& notice) { seen.push_back(notice); });

    const Entity viewer = simulation.World().Spawn(10, 10);
    simulation.World().registry.Assign<ViewerComponent>(viewer, 5);

    const Entity nearby = simulation.World().Spawn(12, 10);
    simulation.World().registry.Assign<DespawnTimerComponent>(nearby, kStep);

    const Entity distant = simulation.World().Spawn(40, 40);
    simulation.World().registry.Assign<DespawnTimerComponent>(distant, kStep);

    simulation.Tick(kStep);

    std::size_t despawnNotices = 0;
    for (const Notice& notice : seen)
    {
        if (notice.kind == NoticeKind::Despawned)
        {
            ++despawnNotices;
            CHECK_EQ(notice.subject, nearby);
            CHECK_EQ(notice.x, 12);
            CHECK_EQ(notice.y, 10);
        }
    }

    // The one in sight, and not the one across the map.
    CHECK_EQ(despawnNotices, std::size_t{1});
}

void ATimerOnNothingInParticularIsHarmless()
{
    // The component carries no assumption about what it is attached to --
    // no position, no item, no health. A bare entity with a clock is a
    // valid thing.
    Simulation simulation(16, 16, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<DespawnRulesModule>();

    const Entity bare = simulation.World().registry.Create();
    simulation.World().registry.Assign<DespawnTimerComponent>(bare, kStep);

    std::vector<EntityExpiredEvent> expired;
    simulation.World().events.Listen<EntityExpiredEvent>(
        [&expired](const EntityExpiredEvent& event) { expired.push_back(event); });

    simulation.Tick(kStep);

    CHECK(!simulation.World().registry.Exists(bare));
    CHECK_EQ(expired.size(), std::size_t{1});
    if (expired.size() == 1)
    {
        // No position to report, so zeroes rather than a lookup on a
        // component that was never there.
        CHECK_EQ(expired[0].x, 0);
        CHECK_EQ(expired[0].y, 0);
    }
}

void ManyTimersExpireInOneTick()
{
    Simulation simulation(64, 64, true);
    simulation.Install<CoreSimulationModule>();
    simulation.Install<BroadcastModule>();
    simulation.Install<DespawnRulesModule>();

    // Staggered, so a whole spread retires over several ticks rather than
    // all at once -- the shape a floor full of loot actually has.
    for (int i = 0; i < 40; ++i)
    {
        const Entity item = SpawnGroundItem(simulation.World(), kPotion, 1, 10, i % 8, i / 8);
        CHECK(item != kNullEntity);
        simulation.World().registry.Assign<DespawnTimerComponent>(item, kStep * static_cast<float>(1 + (i % 4)));
    }

    CHECK_EQ(simulation.World().registry.AliveCount(), std::size_t{40});

    simulation.Tick(kStep);
    CHECK_EQ(simulation.World().registry.AliveCount(), std::size_t{30});

    for (int i = 0; i < 3; ++i)
    {
        simulation.Tick(kStep);
    }

    CHECK_EQ(simulation.World().registry.AliveCount(), std::size_t{0});
    CHECK_EQ(simulation.World().registry.Count<DespawnTimerComponent>(), std::size_t{0});
}

} // namespace

int main()
{
    TheTimerCountsDownAndRemoves();
    ZeroMeansNextTickNotAlreadyExpired();
    AnExpiryIsAnnouncedOnlyOnce();
    APickupOnTheExpiryTickIsNotADoubleRemove();
    AnExpiryPaysNobodyAndDropsNothing();
    ADeathOnTheSameTickWins();
    ExpiryIsBroadcastToViewersInRange();
    ATimerOnNothingInParticularIsHarmless();
    ManyTimersExpireInOneTick();

    return world_v2::test::Summary("DespawnSystem");
}
