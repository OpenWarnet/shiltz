#pragma once

#include "../core/Entity.h"

#include <cstdint>

namespace world_v2
{

// Why a pickup did not happen. Worth telling the client about -- "your bag
// is full" and "someone beat you to it" look identical from the outside
// otherwise, and both are things a player will ask about.
enum class PickupFailure : std::uint8_t
{
    // The item is gone: already taken this tick, already despawned, or the
    // handle never named an item at all.
    Gone,

    // Too far away when the request was resolved. The picker may well have
    // been in range when the packet was sent -- movement runs first.
    OutOfRange,

    // The picker has no InventoryComponent. A wiring mistake rather than a
    // gameplay outcome, kept distinct so it does not hide as a full bag.
    NoInventory,

    // It would not fit.
    InventoryFull,
};

// A ground item was placed on the map.
struct GroundItemSpawnedEvent
{
    Entity entity = kNullEntity;
    std::uint32_t itemId = 0;
    std::uint32_t quantity = 0;
    int x = 0;
    int y = 0;
};

// An item moved from the ground into someone's inventory.
//
// Emitted after the transfer, so by the time a handler sees it the picker
// already holds the goods and the ground item is claimed. Despawning the
// now-empty item entity is what the standard handler does with this -- see
// ItemRules.h.
//
// Carries its own coordinates, like every other event a broadcast is built
// from: the item entity is removed at the barrier, so a listener asking the
// registry where it was would find nothing.
struct ItemPickedUpEvent
{
    Entity picker = kNullEntity;
    Entity item = kNullEntity;
    std::uint32_t itemId = 0;
    std::uint32_t quantity = 0;
    int x = 0;
    int y = 0;
};

struct ItemPickupFailedEvent
{
    Entity picker = kNullEntity;
    Entity item = kNullEntity;
    PickupFailure reason = PickupFailure::Gone;
};

} // namespace world_v2
