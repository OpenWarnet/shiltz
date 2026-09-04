#include "../component/Items.h"
#include "../core/Test.h"
#include "Inventory.h"

#include <cstdint>

using namespace world_v2;

namespace
{

constexpr std::uint32_t kPotion = 100;
constexpr std::uint32_t kSword = 200;

void FreshInventoryIsEmptyButSized()
{
    const InventoryComponent inventory = MakeInventory(5);

    // Fixed-length from the start: five slots, all of them unused. An
    // unused slot is a slot with nothing in it, not a missing element.
    CHECK_EQ(inventory.slots.size(), 5u);
    CHECK_EQ(FreeSlotCount(inventory), 5u);
    CHECK_EQ(CountItem(inventory, kPotion), 0u);
}

void AddingOpensAStack()
{
    InventoryComponent inventory = MakeInventory(4);

    CHECK(TryAddItem(inventory, kPotion, 3, 10));
    CHECK_EQ(CountItem(inventory, kPotion), 3u);
    CHECK_EQ(FreeSlotCount(inventory), 3u);
    CHECK_EQ(inventory.slots[0].itemId, kPotion);
    CHECK_EQ(inventory.slots[0].quantity, 3u);
}

void AddingTopsUpBeforeOpeningNewStacks()
{
    InventoryComponent inventory = MakeInventory(4);

    CHECK(TryAddItem(inventory, kPotion, 6, 10));
    CHECK(TryAddItem(inventory, kPotion, 3, 10));

    // Nine potions in one stack, not two stacks of six and three. Opening a
    // fresh stack while an existing one has room is how an inventory ends
    // up full of half-empty piles of the same thing.
    CHECK_EQ(CountItem(inventory, kPotion), 9u);
    CHECK_EQ(FreeSlotCount(inventory), 3u);
    CHECK_EQ(inventory.slots[0].quantity, 9u);
}

void AddingSpillsIntoTheNextSlotWhenAStackFills()
{
    InventoryComponent inventory = MakeInventory(4);

    CHECK(TryAddItem(inventory, kPotion, 25, 10));

    // 10 + 10 + 5.
    CHECK_EQ(CountItem(inventory, kPotion), 25u);
    CHECK_EQ(FreeSlotCount(inventory), 1u);
    CHECK_EQ(inventory.slots[0].quantity, 10u);
    CHECK_EQ(inventory.slots[1].quantity, 10u);
    CHECK_EQ(inventory.slots[2].quantity, 5u);
}

void DifferentItemsDoNotShareAStack()
{
    InventoryComponent inventory = MakeInventory(4);

    CHECK(TryAddItem(inventory, kPotion, 4, 10));
    CHECK(TryAddItem(inventory, kSword, 1, 1));

    CHECK_EQ(CountItem(inventory, kPotion), 4u);
    CHECK_EQ(CountItem(inventory, kSword), 1u);
    CHECK_EQ(FreeSlotCount(inventory), 2u);
}

void AddingIsAllOrNothing()
{
    InventoryComponent inventory = MakeInventory(2);
    CHECK(TryAddItem(inventory, kPotion, 15, 10));

    // Two slots, ten to a stack: room for exactly twenty, and five already
    // spoken for.
    CHECK(!CanFit(inventory, kPotion, 6, 10));
    CHECK(!TryAddItem(inventory, kPotion, 6, 10));

    // The refusal left nothing behind -- no partial transfer, no slot
    // quietly topped up on the way to failing.
    CHECK_EQ(CountItem(inventory, kPotion), 15u);
    CHECK_EQ(inventory.slots[0].quantity, 10u);
    CHECK_EQ(inventory.slots[1].quantity, 5u);

    // What would have fit is still reportable, for a caller that wants to
    // offer a partial transfer of its own.
    CHECK_EQ(FittableAmount(inventory, kPotion, 6, 10), 5u);

    // And exactly filling it works.
    CHECK(TryAddItem(inventory, kPotion, 5, 10));
    CHECK_EQ(CountItem(inventory, kPotion), 20u);
    CHECK_EQ(FreeSlotCount(inventory), 0u);
}

void AFullInventoryRefusesANewItemType()
{
    InventoryComponent inventory = MakeInventory(2);
    CHECK(TryAddItem(inventory, kPotion, 20, 10));
    CHECK_EQ(FreeSlotCount(inventory), 0u);

    // No empty slot and no stack of swords to top up.
    CHECK(!TryAddItem(inventory, kSword, 1, 10));
    CHECK_EQ(CountItem(inventory, kSword), 0u);

    // But there is still room in the potion stacks... except there is not.
    CHECK(!TryAddItem(inventory, kPotion, 1, 10));
}

void ZeroSizedAndZeroQuantity()
{
    InventoryComponent none = MakeInventory(0);
    CHECK(!TryAddItem(none, kPotion, 1, 10));

    // Asking to add nothing succeeds and does nothing -- even into an
    // inventory with no slots at all.
    CHECK(TryAddItem(none, kPotion, 0, 10));

    InventoryComponent inventory = MakeInventory(2);
    CHECK(TryAddItem(inventory, kPotion, 0, 10));
    CHECK_EQ(FreeSlotCount(inventory), 2u);
}

void AZeroMaxStackMeansOnePerSlot()
{
    InventoryComponent inventory = MakeInventory(3);

    // Item data that did not say. Treating it as "nothing fits anywhere" is
    // never what the caller meant.
    CHECK_EQ(EffectiveMaxStack(0), 1u);
    CHECK(TryAddItem(inventory, kSword, 3, 0));
    CHECK_EQ(CountItem(inventory, kSword), 3u);
    CHECK_EQ(FreeSlotCount(inventory), 0u);
    CHECK_EQ(inventory.slots[0].quantity, 1u);

    CHECK(!TryAddItem(inventory, kSword, 1, 0));
}

void RemovingTakesFromEveryStack()
{
    InventoryComponent inventory = MakeInventory(4);
    CHECK(TryAddItem(inventory, kPotion, 25, 10));

    CHECK_EQ(RemoveItem(inventory, kPotion, 12), 12u);
    CHECK_EQ(CountItem(inventory, kPotion), 13u);

    // The emptied slot is cleared, not erased: the slots after it keep
    // their indices, which is what makes a slot number safe to put on the
    // wire.
    CHECK_EQ(inventory.slots.size(), 4u);
}

void RemovingIsPartialAndReportsWhatItTook()
{
    InventoryComponent inventory = MakeInventory(4);
    CHECK(TryAddItem(inventory, kPotion, 7, 10));

    // Unlike adding: the caller believed it had ten, so take the seven that
    // are there and say so, rather than refusing and leaving them to work
    // out why.
    CHECK_EQ(RemoveItem(inventory, kPotion, 10), 7u);
    CHECK_EQ(CountItem(inventory, kPotion), 0u);
    CHECK_EQ(FreeSlotCount(inventory), 4u);

    CHECK_EQ(RemoveItem(inventory, kPotion, 1), 0u);
    CHECK_EQ(RemoveItem(inventory, kSword, 5), 0u);
}

void RemovingLeavesOtherItemsAlone()
{
    InventoryComponent inventory = MakeInventory(4);
    CHECK(TryAddItem(inventory, kPotion, 5, 10));
    CHECK(TryAddItem(inventory, kSword, 2, 10));

    CHECK_EQ(RemoveItem(inventory, kPotion, 5), 5u);
    CHECK_EQ(CountItem(inventory, kPotion), 0u);
    CHECK_EQ(CountItem(inventory, kSword), 2u);
}

void ClearedSlotsAreReusable()
{
    InventoryComponent inventory = MakeInventory(2);
    CHECK(TryAddItem(inventory, kPotion, 20, 10));
    CHECK_EQ(FreeSlotCount(inventory), 0u);

    CHECK_EQ(RemoveItem(inventory, kPotion, 20), 20u);
    CHECK_EQ(FreeSlotCount(inventory), 2u);

    // A slot emptied by a removal has to be as good as one that was never
    // used -- including its item id, or the next add would think a stack of
    // potions was still sitting there.
    CHECK(TryAddItem(inventory, kSword, 2, 10));
    CHECK_EQ(CountItem(inventory, kSword), 2u);
    CHECK_EQ(CountItem(inventory, kPotion), 0u);
}

} // namespace

int main()
{
    FreshInventoryIsEmptyButSized();
    AddingOpensAStack();
    AddingTopsUpBeforeOpeningNewStacks();
    AddingSpillsIntoTheNextSlotWhenAStackFills();
    DifferentItemsDoNotShareAStack();
    AddingIsAllOrNothing();
    AFullInventoryRefusesANewItemType();
    ZeroSizedAndZeroQuantity();
    AZeroMaxStackMeansOnePerSlot();
    RemovingTakesFromEveryStack();
    RemovingIsPartialAndReportsWhatItTook();
    RemovingLeavesOtherItemsAlone();
    ClearedSlotsAreReusable();

    return world_v2::test::Summary("Inventory");
}
