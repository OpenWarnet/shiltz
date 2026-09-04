#include "../Simulation.h"
#include "../component/Combat.h"
#include "../component/Grid.h"
#include "../component/Network.h"
#include "../component/Despawn.h"
#include "../component/Items.h"
#include "../component/Request.h"
#include "../component/Spawn.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "BroadcastSystem.h"
#include "../world/Inventory.h"
#include "CombatRules.h"
#include "DespawnRules.h"
#include "ItemRules.h"
#include "SpawnRules.h"

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace world_v2;

namespace
{

constexpr int kMonsterFaction = 1;

struct Delivery
{
    Entity viewer = kNullEntity;
    Notice notice;
};

// Collects everything stage 4 hands out, so a test can assert on the exact
// set a given viewer would have been sent.
struct Recorder
{
    std::vector<Delivery> deliveries;

    NoticeSink Sink()
    {
        return [this](Entity viewer, const Notice& notice) { deliveries.push_back(Delivery{viewer, notice}); };
    }

    std::vector<Notice> For(Entity viewer, NoticeKind kind) const
    {
        std::vector<Notice> found;
        for (const Delivery& delivery : deliveries)
        {
            if (delivery.viewer == viewer && delivery.notice.kind == kind)
            {
                found.push_back(delivery.notice);
            }
        }
        return found;
    }

    std::size_t CountFor(Entity viewer) const
    {
        std::size_t total = 0;
        for (const Delivery& delivery : deliveries)
        {
            if (delivery.viewer == viewer)
            {
                ++total;
            }
        }
        return total;
    }
};

Entity AddViewer(Simulation& simulation, int x, int y, int radius)
{
    const Entity viewer = simulation.World().Spawn(x, y);
    simulation.World().registry.Assign<ViewerComponent>(viewer, radius);
    return viewer;
}

void MovementIsAnnouncedWithBothEndpoints()
{
    Simulation simulation(64, 64, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 20, 20, 10);
    const Entity walker = simulation.World().Spawn(22, 20);
    simulation.World().registry.Assign<MoveIntentComponent>(walker, 1, 0);

    simulation.Tick(0.25f);

    const std::vector<Notice> moves = recorder.For(viewer, NoticeKind::Moved);
    CHECK_EQ(moves.size(), 1u);
    if (moves.size() == 1)
    {
        // Both ends and the speed, so a client never has to reconstruct the
        // step by diffing positions between ticks.
        CHECK_EQ(moves[0].subject, walker);
        CHECK_EQ(moves[0].fromX, 22);
        CHECK_EQ(moves[0].fromY, 20);
        CHECK_EQ(moves[0].x, 23);
        CHECK_EQ(moves[0].y, 20);
        CHECK(moves[0].speed > 0.0f);
    }
}

void RejectedMovesAnnounceNothing()
{
    Simulation simulation(64, 64, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 20, 20, 10);
    const Entity walker = simulation.World().Spawn(22, 20);
    simulation.World().tiles.SetWalkable(23, 20, false);
    simulation.World().registry.Assign<MoveIntentComponent>(walker, 1, 0);

    simulation.Tick(0.25f);

    // Nothing happened, so there is nothing to say.
    CHECK_EQ(recorder.For(viewer, NoticeKind::Moved).size(), 0u);
}

void SpawnsAreAnnounced()
{
    Simulation simulation(64, 64, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    InstallSpawnRules(simulation.World(),
                      [](MapWorld& world, Entity monster, std::uint32_t)
                      {
                          world.registry.Assign<FactionComponent>(monster, kMonsterFaction);
                          world.registry.Assign<HealthComponent>(monster, 10, 10);
                      });

    const Entity viewer = AddViewer(simulation, 30, 30, 10);

    const Entity spawner = simulation.World().registry.Create();
    simulation.World().registry.Assign<SpawnerComponent>(spawner, std::uint32_t{77}, 30, 30, 2, 3, 0, 1.0f, 0.0f,
                                                         std::uint32_t{0});

    // Prime resolves its own barrier, so deliver the notices it produced by
    // running a tick.
    simulation.PrimeSpawns();
    simulation.Tick(0.0f);

    const std::vector<Notice> spawns = recorder.For(viewer, NoticeKind::Spawned);
    CHECK_EQ(spawns.size(), 3u);
    for (const Notice& notice : spawns)
    {
        CHECK_EQ(notice.templateId, 77u);
        CHECK(simulation.World().registry.Exists(notice.subject));
    }
}

void DamageIsAnnouncedWithTheNumbers()
{
    Simulation simulation(64, 64, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 20, 20, 10);
    const Entity attacker = simulation.World().Spawn(22, 20);
    const Entity target = simulation.World().Spawn(23, 20);
    simulation.World().registry.Assign<HealthComponent>(target, 50, 50);
    simulation.World().registry.Assign<AttackRequestComponent>(attacker, target, 12);

    simulation.Tick(0.0f);

    const std::vector<Notice> hits = recorder.For(viewer, NoticeKind::Damaged);
    CHECK_EQ(hits.size(), 1u);
    if (hits.size() == 1)
    {
        CHECK_EQ(hits[0].subject, target);
        CHECK_EQ(hits[0].actor, attacker);
        CHECK_EQ(hits[0].amount, 12);
        CHECK_EQ(hits[0].remaining, 38);
        CHECK_EQ(hits[0].x, 23);
        CHECK_EQ(hits[0].y, 20);
    }
}

void DeathIsAnnouncedAfterTheCorpseIsGone()
{
    Simulation simulation(64, 64, true);
    InstallCombatRules(simulation.World());
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 20, 20, 10);
    const Entity attacker = simulation.World().Spawn(22, 20);
    const Entity target = simulation.World().Spawn(23, 20);
    simulation.World().registry.Assign<HealthComponent>(target, 5, 5);
    simulation.World().registry.Assign<AttackRequestComponent>(attacker, target, 5);

    simulation.Tick(0.0f);

    // The barrier despawned the corpse before stage 4 ran. A notice that
    // had to look its position up would be unfilterable exactly here; this
    // one carries it.
    CHECK(!simulation.World().registry.Exists(target));

    const std::vector<Notice> deaths = recorder.For(viewer, NoticeKind::Died);
    CHECK_EQ(deaths.size(), 1u);
    if (deaths.size() == 1)
    {
        CHECK_EQ(deaths[0].subject, target);
        CHECK_EQ(deaths[0].actor, attacker);
        CHECK_EQ(deaths[0].x, 23);
        CHECK_EQ(deaths[0].y, 20);
    }
}

void LootOnTheFloorIsAnnounced()
{
    // The client cannot be told an item vanished if it was never told the
    // item existed. This is the appearance half of that pair.
    Simulation simulation(32, 32, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 10, 10, 6);

    SpawnGroundItem(simulation.World(), 1042, 7, 10, 12, 11);
    simulation.Tick(0.0f);

    const std::vector<Notice> appeared = recorder.For(viewer, NoticeKind::ItemAppeared);
    CHECK_EQ(appeared.size(), std::size_t{1});
    if (appeared.size() == 1)
    {
        CHECK_EQ(appeared[0].x, 12);
        CHECK_EQ(appeared[0].y, 11);

        // An item is an id and a quantity, which is why this is its own
        // kind rather than Spawned.
        CHECK_EQ(appeared[0].templateId, std::uint32_t{1042});
        CHECK_EQ(appeared[0].amount, 7);
    }
}

void LootOutOfSightIsNotAnnounced()
{
    Simulation simulation(64, 64, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 10, 10, 4);

    SpawnGroundItem(simulation.World(), 1042, 1, 10, 40, 40);
    simulation.Tick(0.0f);

    CHECK_EQ(recorder.For(viewer, NoticeKind::ItemAppeared).size(), std::size_t{0});
}

void APickupIsAnnouncedAsARemovalWithItsTaker()
{
    // A pickup is a removal to everyone watching -- the item leaves the
    // floor. `actor` is what lets a client show who took it instead of the
    // thing blinking out.
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 10, 10, 6);

    const Entity picker = simulation.World().Spawn(11, 10);
    simulation.World().registry.Assign<InventoryComponent>(picker, MakeInventory(4));

    const Entity item = SpawnGroundItem(simulation.World(), 1042, 3, 10, 12, 10);
    CHECK(item != kNullEntity);

    // Let the appearance land first, so this tick is only the removal.
    simulation.Tick(0.0f);
    recorder.deliveries.clear();

    simulation.World().registry.Assign<PickupItemRequestComponent>(picker, item);
    simulation.Tick(0.0f);

    const std::vector<Notice> gone = recorder.For(viewer, NoticeKind::Despawned);
    CHECK_EQ(gone.size(), std::size_t{1});
    if (gone.size() == 1)
    {
        CHECK_EQ(gone[0].subject, item);
        CHECK_EQ(gone[0].actor, picker);
        CHECK_EQ(gone[0].x, 12);
        CHECK_EQ(gone[0].y, 10);
    }
}

void ATimeoutIsAnnouncedWithNoTaker()
{
    // The other way an item leaves: nobody took it. Same kind, but no
    // actor -- a client showing "X picked up Y" must not invent an X.
    Simulation simulation(32, 32, true);
    InstallDespawnRules(simulation.World());

    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 10, 10, 6);
    const Entity item = SpawnGroundItem(simulation.World(), 1042, 1, 10, 12, 10);
    simulation.World().registry.Assign<DespawnTimerComponent>(item, 0.25f);

    simulation.Tick(0.0f);
    recorder.deliveries.clear();

    simulation.Tick(0.25f);

    const std::vector<Notice> gone = recorder.For(viewer, NoticeKind::Despawned);
    CHECK_EQ(gone.size(), std::size_t{1});
    if (gone.size() == 1)
    {
        CHECK_EQ(gone[0].subject, item);
        CHECK_EQ(gone[0].actor, kNullEntity);
    }
}

void EveryItemThatAppearsAlsoLeaves()
{
    // The pairing itself, over a whole item lifetime: appeared once, gone
    // once, in that order, to the same viewer. An item stream that only
    // ever announced one half would leave the client either drawing ghosts
    // or missing loot entirely.
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());
    InstallDespawnRules(simulation.World());

    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 10, 10, 8);
    const Entity item = SpawnGroundItem(simulation.World(), 1042, 1, 10, 12, 10);
    simulation.World().registry.Assign<DespawnTimerComponent>(item, 0.25f);

    simulation.Tick(0.0f);
    simulation.Tick(0.25f);

    CHECK_EQ(recorder.For(viewer, NoticeKind::ItemAppeared).size(), std::size_t{1});
    CHECK_EQ(recorder.For(viewer, NoticeKind::Despawned).size(), std::size_t{1});

    // Order matters: a removal before the appearance is unusable.
    std::size_t appearedAt = 0;
    std::size_t goneAt = 0;
    for (std::size_t i = 0; i < recorder.deliveries.size(); ++i)
    {
        if (recorder.deliveries[i].notice.kind == NoticeKind::ItemAppeared)
        {
            appearedAt = i;
        }
        if (recorder.deliveries[i].notice.kind == NoticeKind::Despawned)
        {
            goneAt = i;
        }
    }
    CHECK(appearedAt < goneAt);
}

void ListenerOrderDoesNotMatter()
{
    // The same death, with the corpse-removing handler registered *before*
    // the notice collector. Every event these read carries its own
    // coordinates, so the result has to be identical.
    MapWorld world(64, 64, true);
    InstallCombatRules(world);

    BroadcastSystem broadcast;
    broadcast.Install(world);

    CombatSystem combat;
    DeathSystem death;

    const Entity attacker = world.Spawn(10, 10);
    const Entity target = world.Spawn(11, 10);
    world.registry.Assign<HealthComponent>(target, 5, 5);
    world.registry.Assign<AttackRequestComponent>(attacker, target, 5);

    combat.Update(world.registry, world.events);
    death.Update(world.registry, world.events);
    world.events.Flush();

    CHECK(!world.registry.Exists(target));

    int deaths = 0;
    for (const Notice& notice : broadcast.Pending())
    {
        if (notice.kind == NoticeKind::Died)
        {
            ++deaths;
            CHECK_EQ(notice.x, 11);
            CHECK_EQ(notice.y, 10);
        }
    }
    CHECK_EQ(deaths, 1);
}

void ViewersOnlySeeWhatIsNear()
{
    Simulation simulation(128, 128, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity close = AddViewer(simulation, 20, 20, 5);
    const Entity far = AddViewer(simulation, 100, 100, 5);

    const Entity walker = simulation.World().Spawn(22, 20);
    simulation.World().registry.Assign<MoveIntentComponent>(walker, 1, 0);

    simulation.Tick(0.25f);

    CHECK_EQ(recorder.For(close, NoticeKind::Moved).size(), 1u);
    CHECK_EQ(recorder.For(far, NoticeKind::Moved).size(), 0u);
    CHECK_EQ(recorder.CountFor(far), 0u);
}

void AViewerHearsAboutItselfAndNothingElse()
{
    Simulation simulation(128, 128, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    // Radius zero: this viewer sees nothing but its own doings.
    const Entity viewer = AddViewer(simulation, 20, 20, 0);
    simulation.World().registry.Assign<MoveIntentComponent>(viewer, 1, 0);

    const Entity stranger = simulation.World().Spawn(25, 20);
    simulation.World().registry.Assign<MoveIntentComponent>(stranger, 1, 0);

    simulation.Tick(0.25f);

    const std::vector<Notice> moves = recorder.For(viewer, NoticeKind::Moved);
    CHECK_EQ(moves.size(), 1u);
    if (moves.size() == 1)
    {
        CHECK_EQ(moves[0].subject, viewer);
    }

    // Note on what this does and does not prove: it passes with the
    // explicit self-visibility rule removed, because every notice kind
    // carries its subject's own position, so a viewer is within any radius
    // of a notice about itself. The rule in IsVisibleTo is a guarantee held
    // in reserve for a future notice kind that breaks that, not something
    // this case can distinguish.
}

void ADespawnedViewerHearsNothing()
{
    Simulation simulation(64, 64, true);
    InstallCombatRules(simulation.World());
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 20, 20, 10);
    simulation.World().registry.Assign<HealthComponent>(viewer, 5, 5);

    const Entity attacker = simulation.World().Spawn(21, 20);
    simulation.World().registry.Assign<AttackRequestComponent>(attacker, viewer, 5);

    const Entity bystander = AddViewer(simulation, 22, 20, 10);

    simulation.Tick(0.0f);

    // The default death rule despawns the corpse at the barrier, so by the
    // time delivery runs the viewer is no longer in the registry and gets
    // nothing -- not even its own death. Pinned down here because it is a
    // real limitation rather than an oversight: a game where players should
    // hear their own death has to replace that rule with one that leaves
    // the player entity in place.
    CHECK(!simulation.World().registry.Exists(viewer));
    CHECK_EQ(recorder.CountFor(viewer), 0u);

    // Everyone still present hears about it normally.
    const std::vector<Notice> deaths = recorder.For(bystander, NoticeKind::Died);
    CHECK_EQ(deaths.size(), 1u);
    if (deaths.size() == 1)
    {
        CHECK_EQ(deaths[0].subject, viewer);
    }
}

void AStepOutOfRangeIsStillDelivered()
{
    Simulation simulation(128, 128, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 20, 20, 3);

    // Standing at the edge of vision and stepping out of it. Without the
    // origin end of the step counting, the creature would freeze on screen
    // at the boundary instead of walking away.
    const Entity leaver = simulation.World().Spawn(23, 20);
    simulation.World().registry.Assign<MoveIntentComponent>(leaver, 1, 0);

    simulation.Tick(0.25f);

    const std::vector<Notice> moves = recorder.For(viewer, NoticeKind::Moved);
    CHECK_EQ(moves.size(), 1u);
    if (moves.size() == 1)
    {
        CHECK_EQ(moves[0].fromX, 23);
        CHECK_EQ(moves[0].x, 24);
    }
}

void NoticesDoNotCarryOverBetweenTicks()
{
    Simulation simulation(64, 64, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity viewer = AddViewer(simulation, 20, 20, 10);
    const Entity walker = simulation.World().Spawn(22, 20);
    simulation.World().registry.Assign<MoveIntentComponent>(walker, 1, 0);

    simulation.Tick(0.25f);
    CHECK_EQ(recorder.For(viewer, NoticeKind::Moved).size(), 1u);
    CHECK_EQ(simulation.Broadcast().Pending().size(), 0u);

    // A quiet tick says nothing, rather than repeating the last one.
    simulation.Tick(0.25f);
    CHECK_EQ(recorder.For(viewer, NoticeKind::Moved).size(), 1u);
}

void AnUnwatchedSimulationDoesNotAccumulate()
{
    Simulation simulation(64, 64, true);

    // No sink at all. The notices still have to be cleared, or an
    // unwatched map grows a list forever.
    const Entity walker = simulation.World().Spawn(22, 20);

    for (int i = 0; i < 10; ++i)
    {
        simulation.World().registry.Assign<MoveIntentComponent>(walker, 1, 0);
        simulation.Tick(0.25f);
        CHECK_EQ(simulation.Broadcast().Pending().size(), 0u);
    }
}

void EachViewerGetsItsOwnSet()
{
    Simulation simulation(128, 128, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    const Entity west = AddViewer(simulation, 20, 20, 4);
    const Entity east = AddViewer(simulation, 40, 20, 4);

    const Entity nearWest = simulation.World().Spawn(22, 20);
    simulation.World().registry.Assign<MoveIntentComponent>(nearWest, 0, 1);

    const Entity nearEast = simulation.World().Spawn(42, 20);
    simulation.World().registry.Assign<MoveIntentComponent>(nearEast, 0, 1);

    simulation.Tick(0.25f);

    const std::vector<Notice> westMoves = recorder.For(west, NoticeKind::Moved);
    const std::vector<Notice> eastMoves = recorder.For(east, NoticeKind::Moved);

    CHECK_EQ(westMoves.size(), 1u);
    CHECK_EQ(eastMoves.size(), 1u);
    if (westMoves.size() == 1 && eastMoves.size() == 1)
    {
        CHECK_EQ(westMoves[0].subject, nearWest);
        CHECK_EQ(eastMoves[0].subject, nearEast);
    }
}

void ANonViewerReceivesNothing()
{
    Simulation simulation(64, 64, true);
    Recorder recorder;
    simulation.OnNotice(recorder.Sink());

    // On the map, right next to the action, but nobody is watching through
    // it -- a monster, not a player.
    const Entity bystander = simulation.World().Spawn(21, 20);
    const Entity walker = simulation.World().Spawn(22, 20);
    simulation.World().registry.Assign<MoveIntentComponent>(walker, 1, 0);

    simulation.Tick(0.25f);

    CHECK_EQ(recorder.CountFor(bystander), 0u);
    CHECK_EQ(recorder.deliveries.size(), 0u);
}

} // namespace

int main()
{
    MovementIsAnnouncedWithBothEndpoints();
    RejectedMovesAnnounceNothing();
    SpawnsAreAnnounced();
    DamageIsAnnouncedWithTheNumbers();
    DeathIsAnnouncedAfterTheCorpseIsGone();
    LootOnTheFloorIsAnnounced();
    LootOutOfSightIsNotAnnounced();
    APickupIsAnnouncedAsARemovalWithItsTaker();
    ATimeoutIsAnnouncedWithNoTaker();
    EveryItemThatAppearsAlsoLeaves();
    ListenerOrderDoesNotMatter();
    ViewersOnlySeeWhatIsNear();
    AViewerHearsAboutItselfAndNothingElse();
    ADespawnedViewerHearsNothing();
    AStepOutOfRangeIsStillDelivered();
    NoticesDoNotCarryOverBetweenTicks();
    AnUnwatchedSimulationDoesNotAccumulate();
    EachViewerGetsItsOwnSet();
    ANonViewerReceivesNothing();

    return world_v2::test::Summary("BroadcastSystem");
}
