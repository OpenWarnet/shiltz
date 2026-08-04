#pragma once

#include <cstdint>

// A single item instance sitting on the ground, tracked by the simulation
// -- not the same thing as an inventory_slot DB row (which is a
// character's owned/stored item). `id` is the random per-drop instance
// id handed to the client via ItemMapNew.id and echoed back in
// CG_ITEM_PICKUP.id.
struct Item
{
    std::uint32_t id = 0;
    std::uint32_t item_id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t quantity = 0;
    std::uint32_t refine_level = 0;
};
