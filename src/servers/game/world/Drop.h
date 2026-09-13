#pragma once

#include "Item.h"

#include <chrono>
#include <cstdint>

// How long an unclaimed Drop stays on the ground before Map::TickDrops despawns it.
inline constexpr std::chrono::milliseconds kDefaultDropLifetime{120'000};

// A single item sitting on the ground, tracked by the simulation -- not
// the same thing as an inventory_slot DB row (which is a character's
// owned/stored item). `id` is the per-drop entity id (EntityIdGenerator::Next())
// handed to the client via ItemMapNew.id and echoed back in CG_ITEM_PICKUP.id.
// Embeds Item (rather than just item_id/quantity/refine_level) so a
// dropped item's item_level/option_bits survive the ground stage instead
// of being lost between drop and pickup.
struct Drop
{
    std::uint32_t id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    Item item;

    // Counts down by each Map::Tick's delta (see Creature::ai_timer for the same idiom);
    // Map::TickDrops despawns this once it reaches zero.
    std::chrono::milliseconds time_to_live = kDefaultDropLifetime;
};
