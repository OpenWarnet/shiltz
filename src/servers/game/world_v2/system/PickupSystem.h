#pragma once

#include "../component/Grid.h"
#include "../component/Items.h"
#include "../component/Request.h"
#include "../core/Entity.h"
#include "../core/EventManager.h"
#include "../core/Registry.h"
#include "../core/System.h"
#include "../event/ItemEvents.h"
#include "../world/Inventory.h"

#include <cstdlib>

namespace world_v2
{

// Resolves this tick's pickup requests.
//
// Every request is consumed whether or not it succeeds, for the same reason
// movement consumes intents and combat consumes attacks: one that survived
// rejection would retry forever, and one that survived success would take
// the item twice.
//
// Requests are re-validated here rather than trusted. A player may have
// been standing on the item when the packet was sent and have walked off it
// since -- movement runs earlier in the same tick -- and a request that came
// straight from a client was never checked at all.
//
// The duplication guard
// ---------------------
// Two players requesting the same item in the same tick is not an edge case,
// it is what happens every time something good drops. Both requests are
// resolved in this one sweep, so "does it still exist" is not enough on its
// own: the entity is not removed until the barrier, well after both have
// been looked at.
//
// So taking an item sets its quantity to zero, immediately, in the same
// breath as adding it to the inventory. The second picker looks at the same
// entity a moment later, sees nothing left on it, and is told it is gone.
// The entity itself is cleaned up at the barrier by the standard handler
// for ItemPickedUpEvent.
class PickupSystem : public ISystem
{
public:
    const char* Name() const override
    {
        return "PickupSystem";
    }

    // Stage-2 adapter. Update below is the real entry point, and its
    // signature is what declares which parts of the Map this touches.
    void Run(Map& world, float deltaSeconds) override
    {
        (void)deltaSeconds;
        Update(world.registry, world.events);
    }

    // Chebyshev tiles. One means the item may be underfoot or on any
    // adjacent tile; zero would require standing exactly on it.
    int pickupRange = 1;

    void Update(Registry& registry, EventManager& events)
    {
        // Driven off the request pool alone rather than a wider view, so a
        // picker missing a position or an inventory still gets its request
        // consumed and an answer -- rather than leaving a request nothing
        // ever looks at again.
        registry.view<PickupItemRequestComponent>().Each(
            [&](Entity picker, PickupItemRequestComponent& request)
            {
                // Read before consuming: the Remove below leaves `request`
                // dangling.
                const Entity item = request.targetItemEntity;

                // The current entity, in the pool being iterated -- the one
                // structural change a view documents as safe.
                registry.Remove<PickupItemRequestComponent>(picker);

                if (!registry.Exists(item) || !registry.Exists(picker))
                {
                    events.Emit(ItemPickupFailedEvent{picker, item, PickupFailure::Gone});
                    return;
                }

                GroundItemComponent* ground = registry.TryGet<GroundItemComponent>(item);
                if (ground == nullptr || ground->quantity == 0)
                {
                    // Not an item, or already claimed earlier in this very
                    // sweep.
                    events.Emit(ItemPickupFailedEvent{picker, item, PickupFailure::Gone});
                    return;
                }

                const GridPositionComponent* itemPosition = registry.TryGet<GridPositionComponent>(item);
                const GridPositionComponent* pickerPosition = registry.TryGet<GridPositionComponent>(picker);
                if (itemPosition == nullptr || pickerPosition == nullptr)
                {
                    events.Emit(ItemPickupFailedEvent{picker, item, PickupFailure::Gone});
                    return;
                }

                const int dx = std::abs(itemPosition->x - pickerPosition->x);
                const int dy = std::abs(itemPosition->y - pickerPosition->y);
                if ((dx > dy ? dx : dy) > pickupRange)
                {
                    events.Emit(ItemPickupFailedEvent{picker, item, PickupFailure::OutOfRange});
                    return;
                }

                InventoryComponent* inventory = registry.TryGet<InventoryComponent>(picker);
                if (inventory == nullptr)
                {
                    events.Emit(ItemPickupFailedEvent{picker, item, PickupFailure::NoInventory});
                    return;
                }

                const std::uint32_t itemId = ground->itemId;
                const std::uint32_t quantity = ground->quantity;
                const int x = itemPosition->x;
                const int y = itemPosition->y;

                if (!TryAddItem(*inventory, itemId, quantity, ground->maxStack))
                {
                    events.Emit(ItemPickupFailedEvent{picker, item, PickupFailure::InventoryFull});
                    return;
                }

                // Claim it in the same breath as taking it. Everything
                // above this line can fail; nothing below it can, so the
                // item is never marked taken by a pickup that did not
                // happen.
                ground->quantity = 0;

                events.Emit(ItemPickedUpEvent{picker, item, itemId, quantity, x, y});
            });
    }
};

} // namespace world_v2
