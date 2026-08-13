#pragma once

#include "Item.h"

#include <cstdint>

// A single item sitting on the ground, tracked by the simulation -- not
// the same thing as an inventory_slot DB row (which is a character's
// owned/stored item). `id` is the random per-drop instance id handed to
// the client via ItemMapNew.id and echoed back in CG_ITEM_PICKUP.id.
// Embeds Item (rather than just item_id/quantity/refine_level) so a
// dropped item's item_level/option_bits survive the ground stage instead
// of being lost between drop and pickup.
struct GroundItem
{
    std::uint32_t id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    Item item;
};
