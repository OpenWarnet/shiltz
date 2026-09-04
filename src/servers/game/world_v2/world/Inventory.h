#pragma once

#include "../component/Items.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace world_v2
{

// Slot arithmetic for InventoryComponent.
//
// Free functions rather than methods, and no registry, no entities, no
// events: this is the one part of the item system that is pure data in,
// data out, so it can be reasoned about and tested without a world existing
// at all. PickupSystem is what connects it to one.
//
// Every function here keeps the invariant InventoryComponent documents --
// slots is fixed-length, an unused slot is one with quantity zero, and a
// slot index never shifts because an earlier stack emptied.

// A maxStack of zero would mean nothing fits anywhere, which is never what
// a caller meant -- it means the item data did not say. One per slot is the
// conservative reading.
inline std::uint32_t EffectiveMaxStack(std::uint32_t maxStack)
{
    return maxStack == 0 ? 1u : maxStack;
}

inline InventoryComponent MakeInventory(std::size_t slotCount)
{
    InventoryComponent inventory;
    inventory.slots.resize(slotCount);
    return inventory;
}

inline std::size_t FreeSlotCount(const InventoryComponent& inventory)
{
    std::size_t free = 0;
    for (const InventoryComponent::Slot& slot : inventory.slots)
    {
        if (slot.IsEmpty())
        {
            ++free;
        }
    }
    return free;
}

// Total held across every stack, not the number of stacks.
inline std::uint32_t CountItem(const InventoryComponent& inventory, std::uint32_t itemId)
{
    std::uint32_t total = 0;
    for (const InventoryComponent::Slot& slot : inventory.slots)
    {
        if (!slot.IsEmpty() && slot.itemId == itemId)
        {
            total += slot.quantity;
        }
    }
    return total;
}

// How much of `quantity` would fit, topping up existing stacks first and
// then filling empty slots.
//
// Existing stacks first is not a preference -- it is what stops an
// inventory from filling with half-empty stacks of the same thing while
// reporting itself full.
inline std::uint32_t FittableAmount(const InventoryComponent& inventory, std::uint32_t itemId, std::uint32_t quantity,
                                    std::uint32_t maxStack)
{
    const std::uint32_t stackLimit = EffectiveMaxStack(maxStack);
    std::uint32_t remaining = quantity;

    for (const InventoryComponent::Slot& slot : inventory.slots)
    {
        if (remaining == 0)
        {
            return quantity;
        }

        if (slot.IsEmpty() || slot.itemId != itemId || slot.quantity >= stackLimit)
        {
            continue;
        }

        const std::uint32_t space = stackLimit - slot.quantity;
        remaining -= space < remaining ? space : remaining;
    }

    for (const InventoryComponent::Slot& slot : inventory.slots)
    {
        if (remaining == 0)
        {
            break;
        }

        if (slot.IsEmpty())
        {
            remaining -= stackLimit < remaining ? stackLimit : remaining;
        }
    }

    return quantity - remaining;
}

inline bool CanFit(const InventoryComponent& inventory, std::uint32_t itemId, std::uint32_t quantity,
                   std::uint32_t maxStack)
{
    return FittableAmount(inventory, itemId, quantity, maxStack) == quantity;
}

// Adds the whole amount or nothing at all.
//
// All-or-nothing on purpose. A partial add would have to leave the
// remainder somewhere -- back on the ground, in a fresh stack, dropped --
// and every one of those is a gameplay decision, not a data-structure one.
// Refusing outright means an inventory is never left in a state nobody
// asked for, and the caller can offer a partial variant later on top of
// FittableAmount without this having guessed first.
//
// A quantity of zero succeeds and does nothing.
inline bool TryAddItem(InventoryComponent& inventory, std::uint32_t itemId, std::uint32_t quantity,
                       std::uint32_t maxStack)
{
    if (quantity == 0)
    {
        return true;
    }

    if (!CanFit(inventory, itemId, quantity, maxStack))
    {
        return false;
    }

    const std::uint32_t stackLimit = EffectiveMaxStack(maxStack);
    std::uint32_t remaining = quantity;

    // Top up what is already there...
    for (InventoryComponent::Slot& slot : inventory.slots)
    {
        if (remaining == 0)
        {
            return true;
        }

        if (slot.IsEmpty() || slot.itemId != itemId || slot.quantity >= stackLimit)
        {
            continue;
        }

        const std::uint32_t space = stackLimit - slot.quantity;
        const std::uint32_t moved = space < remaining ? space : remaining;
        slot.quantity += moved;
        remaining -= moved;
    }

    // ...then open new stacks.
    for (InventoryComponent::Slot& slot : inventory.slots)
    {
        if (remaining == 0)
        {
            return true;
        }

        if (!slot.IsEmpty())
        {
            continue;
        }

        const std::uint32_t moved = stackLimit < remaining ? stackLimit : remaining;
        slot.itemId = itemId;
        slot.quantity = moved;
        remaining -= moved;
    }

    // Unreachable: CanFit already said the whole amount would go in.
    return remaining == 0;
}

// Takes up to `quantity` from wherever it is held, and returns how much was
// actually taken.
//
// Partial here, unlike adding, because the caller asked to remove something
// it believed it had -- removing what is there and reporting the shortfall
// is more useful than refusing and leaving them to work out why.
//
// An emptied slot is cleared rather than removed, so the slots after it
// keep their indices.
inline std::uint32_t RemoveItem(InventoryComponent& inventory, std::uint32_t itemId, std::uint32_t quantity)
{
    std::uint32_t remaining = quantity;

    for (InventoryComponent::Slot& slot : inventory.slots)
    {
        if (remaining == 0)
        {
            break;
        }

        if (slot.IsEmpty() || slot.itemId != itemId)
        {
            continue;
        }

        const std::uint32_t taken = slot.quantity < remaining ? slot.quantity : remaining;
        slot.quantity -= taken;
        remaining -= taken;

        if (slot.quantity == 0)
        {
            slot.itemId = 0;
        }
    }

    return quantity - remaining;
}

} // namespace world_v2
