#include "../Simulation.h"
#include "../component/Grid.h"
#include "../component/Items.h"
#include "../component/Request.h"
#include "../core/Entity.h"
#include "../core/Test.h"
#include "../event/ItemEvents.h"
#include "../world/Inventory.h"
#include "ItemRules.h"
#include "PickupSystem.h"

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace world_v2;

namespace
{

constexpr std::uint32_t kPotion = 100;

Entity AddCarrier(Simulation& simulation, int x, int y, std::size_t slots)
{
    const Entity carrier = simulation.World().Spawn(x, y);
    simulation.World().registry.Assign<InventoryComponent>(carrier, MakeInventory(slots));
    return carrier;
}

void PickingUpMovesTheGoodsAndClearsTheGround()
{
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    std::vector<ItemPickedUpEvent> taken;
    simulation.World().events.Listen<ItemPickedUpEvent>([&taken](const ItemPickedUpEvent& e) { taken.push_back(e); });

    const Entity carrier = AddCarrier(simulation, 10, 10, 4);
    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 7, 10, 11, 10);
    CHECK(item != kNullEntity);

    // The item is on the index like anything else with a position, and
    // that stops nobody standing next to or on top of it.
    CHECK(simulation.World().tiles.Contains(item, 11, 10));

    simulation.World().registry.Assign<PickupItemRequestComponent>(carrier, item);
    simulation.Tick(0.0f);

    CHECK_EQ(CountItem(simulation.World().registry.Get<InventoryComponent>(carrier), kPotion), 7u);

    // Removing the husk is structural, so it happens at the barrier.
    CHECK(!simulation.World().registry.Exists(item));
    CHECK(!simulation.World().registry.Has<PickupItemRequestComponent>(carrier));

    CHECK_EQ(taken.size(), 1u);
    if (taken.size() == 1)
    {
        CHECK_EQ(taken[0].picker, carrier);
        CHECK_EQ(taken[0].item, item);
        CHECK_EQ(taken[0].itemId, kPotion);
        CHECK_EQ(taken[0].quantity, 7u);
        CHECK_EQ(taken[0].x, 11);
        CHECK_EQ(taken[0].y, 10);
    }
}

void RequestsAreAlwaysConsumed()
{
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    std::vector<ItemPickupFailedEvent> refused;
    simulation.World().events.Listen<ItemPickupFailedEvent>(
        [&refused](const ItemPickupFailedEvent& e) { refused.push_back(e); });

    // Far away, so it will be refused -- and must not keep retrying.
    const Entity carrier = AddCarrier(simulation, 10, 10, 4);
    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 1, 10, 25, 25);

    simulation.World().registry.Assign<PickupItemRequestComponent>(carrier, item);
    simulation.Tick(0.0f);

    CHECK(!simulation.World().registry.Has<PickupItemRequestComponent>(carrier));
    CHECK_EQ(refused.size(), 1u);
    if (refused.size() == 1)
    {
        CHECK(refused[0].reason == PickupFailure::OutOfRange);
    }

    // Still on the ground for someone who walks over.
    CHECK(simulation.World().registry.Exists(item));
    CHECK_EQ(simulation.World().registry.Get<GroundItemComponent>(item).quantity, 1u);

    simulation.Tick(0.0f);
    CHECK_EQ(refused.size(), 1u);
}

void RangeIsCheckedAgainstWhereThingsEndedUp()
{
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    const Entity carrier = AddCarrier(simulation, 10, 10, 4);
    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 1, 10, 12, 10);

    // Two tiles away, and stepping towards it. Movement runs before pickup
    // in the same tick, so by the time the request is judged the carrier is
    // adjacent -- a player who was out of range when the packet was sent
    // still gets the item.
    simulation.World().registry.Assign<MoveIntentComponent>(carrier, 1, 0);
    simulation.World().registry.Assign<PickupItemRequestComponent>(carrier, item);
    simulation.Tick(0.0f);

    CHECK_EQ(simulation.World().registry.Get<GridPositionComponent>(carrier).x, 11);
    CHECK_EQ(CountItem(simulation.World().registry.Get<InventoryComponent>(carrier), kPotion), 1u);
}

void StandingOnTheItemWorksToo()
{
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    const Entity carrier = AddCarrier(simulation, 10, 10, 4);
    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 2, 10, 10, 10);

    simulation.World().registry.Assign<PickupItemRequestComponent>(carrier, item);
    simulation.Tick(0.0f);

    CHECK_EQ(CountItem(simulation.World().registry.Get<InventoryComponent>(carrier), kPotion), 2u);
}

void AFullBagRefusesAndLeavesTheItem()
{
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    std::vector<ItemPickupFailedEvent> refused;
    simulation.World().events.Listen<ItemPickupFailedEvent>(
        [&refused](const ItemPickupFailedEvent& e) { refused.push_back(e); });

    const Entity carrier = AddCarrier(simulation, 10, 10, 1);
    InventoryComponent& inventory = simulation.World().registry.Get<InventoryComponent>(carrier);
    CHECK(TryAddItem(inventory, kPotion, 10, 10));

    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 5, 10, 11, 10);
    simulation.World().registry.Assign<PickupItemRequestComponent>(carrier, item);
    simulation.Tick(0.0f);

    CHECK_EQ(refused.size(), 1u);
    if (refused.size() == 1)
    {
        CHECK(refused[0].reason == PickupFailure::InventoryFull);
    }

    // A refused pickup must leave the item exactly as it was -- not
    // partially taken, not claimed, not despawned.
    CHECK(simulation.World().registry.Exists(item));
    CHECK_EQ(simulation.World().registry.Get<GroundItemComponent>(item).quantity, 5u);
    CHECK_EQ(CountItem(simulation.World().registry.Get<InventoryComponent>(carrier), kPotion), 10u);
}

void ACarrierWithNoInventorySaysSo()
{
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    std::vector<ItemPickupFailedEvent> refused;
    simulation.World().events.Listen<ItemPickupFailedEvent>(
        [&refused](const ItemPickupFailedEvent& e) { refused.push_back(e); });

    // No InventoryComponent at all -- a wiring mistake, kept distinct from
    // a full bag so it does not hide as one.
    const Entity carrier = simulation.World().Spawn(10, 10);
    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 1, 10, 11, 10);

    simulation.World().registry.Assign<PickupItemRequestComponent>(carrier, item);
    simulation.Tick(0.0f);

    CHECK_EQ(refused.size(), 1u);
    if (refused.size() == 1)
    {
        CHECK(refused[0].reason == PickupFailure::NoInventory);
    }
    CHECK(simulation.World().registry.Exists(item));
}

void TwoPickersCannotBothTakeIt()
{
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    std::vector<ItemPickedUpEvent> taken;
    std::vector<ItemPickupFailedEvent> refused;
    simulation.World().events.Listen<ItemPickedUpEvent>([&taken](const ItemPickedUpEvent& e) { taken.push_back(e); });
    simulation.World().events.Listen<ItemPickupFailedEvent>(
        [&refused](const ItemPickupFailedEvent& e) { refused.push_back(e); });

    const Entity first = AddCarrier(simulation, 10, 10, 4);
    const Entity second = AddCarrier(simulation, 12, 10, 4);
    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 9, 10, 11, 10);

    // Both request it in the same tick, both are adjacent, and both are
    // resolved in one sweep -- well before the entity is removed at the
    // barrier. "Does it still exist" is not enough here; the claim is what
    // stops it being duplicated.
    simulation.World().registry.Assign<PickupItemRequestComponent>(first, item);
    simulation.World().registry.Assign<PickupItemRequestComponent>(second, item);
    simulation.Tick(0.0f);

    CHECK_EQ(taken.size(), 1u);
    CHECK_EQ(refused.size(), 1u);
    if (refused.size() == 1)
    {
        CHECK(refused[0].reason == PickupFailure::Gone);
    }

    // Nine potions existed and nine potions exist. Exactly one carrier has
    // them.
    const std::uint32_t firstHas = CountItem(simulation.World().registry.Get<InventoryComponent>(first), kPotion);
    const std::uint32_t secondHas = CountItem(simulation.World().registry.Get<InventoryComponent>(second), kPotion);
    CHECK_EQ(firstHas + secondHas, 9u);
    CHECK(firstHas == 0u || secondHas == 0u);

    CHECK(!simulation.World().registry.Exists(item));
}

void AStaleItemHandleIsRefused()
{
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    const Entity carrier = AddCarrier(simulation, 10, 10, 4);
    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 3, 10, 11, 10);

    simulation.World().registry.Assign<PickupItemRequestComponent>(carrier, item);
    simulation.World().Despawn(item);

    // The freed slot is taken over by something else lying in the same
    // place. The generation in the handle is what stops the queued request
    // from taking that instead.
    const Entity replacement = SpawnGroundItem(simulation.World(), kPotion, 99, 100, 11, 10);
    CHECK_EQ(EntityIndex(replacement), EntityIndex(item));

    simulation.Tick(0.0f);

    CHECK_EQ(CountItem(simulation.World().registry.Get<InventoryComponent>(carrier), kPotion), 0u);
    CHECK(simulation.World().registry.Exists(replacement));
    CHECK_EQ(simulation.World().registry.Get<GroundItemComponent>(replacement).quantity, 99u);
}

void ItemsDoNotBlockAnything()
{
    Simulation simulation(32, 32, true);
    InstallItemRules(simulation.World());

    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 1, 10, 11, 10);
    CHECK(item != kNullEntity);

    // Several on one square -- all of them on the index, since nothing is
    // hidden from it any more -- and a creature free to walk straight over
    // the pile.
    const Entity second = SpawnGroundItem(simulation.World(), kPotion, 1, 10, 11, 10);
    CHECK(second != kNullEntity);
    CHECK_EQ(simulation.World().tiles.OccupantCount(11, 10), std::size_t{2});

    const Entity walker = simulation.World().Spawn(10, 10);
    simulation.World().registry.Assign<MoveIntentComponent>(walker, 1, 0);
    simulation.Tick(0.25f);

    CHECK_EQ(simulation.World().registry.Get<GridPositionComponent>(walker).x, 11);

    // Standing on the pile, not stopped by it, and neither item disturbed.
    CHECK_EQ(simulation.World().tiles.OccupantCount(11, 10), std::size_t{3});
    CHECK(simulation.World().tiles.Contains(walker, 11, 10));
    CHECK(simulation.World().tiles.Contains(item, 11, 10));
    CHECK(simulation.World().tiles.Contains(second, 11, 10));
}

void SpawningNothingSpawnsNothing()
{
    Simulation simulation(32, 32, true);

    // A pile of zero would read as already-claimed and sit on the map
    // forever.
    CHECK_EQ(SpawnGroundItem(simulation.World(), kPotion, 0, 10, 11, 10), kNullEntity);

    // And off the map is off the map.
    CHECK_EQ(SpawnGroundItem(simulation.World(), kPotion, 1, 10, -1, 10), kNullEntity);
    CHECK_EQ(simulation.World().registry.AliveCount(), 0u);
}

void SpawnIsAnnounced()
{
    Simulation simulation(32, 32, true);

    std::vector<GroundItemSpawnedEvent> dropped;
    simulation.World().events.Listen<GroundItemSpawnedEvent>(
        [&dropped](const GroundItemSpawnedEvent& e) { dropped.push_back(e); });

    const Entity item = SpawnGroundItem(simulation.World(), kPotion, 4, 10, 6, 7);
    simulation.Tick(0.0f);

    CHECK_EQ(dropped.size(), 1u);
    if (dropped.size() == 1)
    {
        CHECK_EQ(dropped[0].entity, item);
        CHECK_EQ(dropped[0].itemId, kPotion);
        CHECK_EQ(dropped[0].quantity, 4u);
        CHECK_EQ(dropped[0].x, 6);
        CHECK_EQ(dropped[0].y, 7);
    }
}

void ManyPickupsInOneTick()
{
    Simulation simulation(64, 64, true);
    InstallItemRules(simulation.World());

    // Each carrier takes its own item, all resolved in one sweep, each
    // removing its own request from the pool being iterated.
    std::vector<Entity> carriers;
    std::vector<Entity> items;
    for (int i = 0; i < 16; ++i)
    {
        const Entity carrier = AddCarrier(simulation, i * 3, 20, 4);
        CHECK(carrier != kNullEntity);
        const Entity item = SpawnGroundItem(simulation.World(), kPotion, 2, 10, i * 3, 21);
        CHECK(item != kNullEntity);

        simulation.World().registry.Assign<PickupItemRequestComponent>(carrier, item);
        carriers.push_back(carrier);
        items.push_back(item);
    }

    simulation.Tick(0.0f);

    CHECK_EQ(simulation.World().registry.Count<PickupItemRequestComponent>(), 0u);
    CHECK_EQ(simulation.World().registry.Count<GroundItemComponent>(), 0u);
    for (const Entity carrier : carriers)
    {
        CHECK_EQ(CountItem(simulation.World().registry.Get<InventoryComponent>(carrier), kPotion), 2u);
    }
    for (const Entity item : items)
    {
        CHECK(!simulation.World().registry.Exists(item));
    }
}

} // namespace

int main()
{
    PickingUpMovesTheGoodsAndClearsTheGround();
    RequestsAreAlwaysConsumed();
    RangeIsCheckedAgainstWhereThingsEndedUp();
    StandingOnTheItemWorksToo();
    AFullBagRefusesAndLeavesTheItem();
    ACarrierWithNoInventorySaysSo();
    TwoPickersCannotBothTakeIt();
    AStaleItemHandleIsRefused();
    ItemsDoNotBlockAnything();
    SpawningNothingSpawnsNothing();
    SpawnIsAnnounced();
    ManyPickupsInOneTick();

    return world_v2::test::Summary("PickupSystem");
}
