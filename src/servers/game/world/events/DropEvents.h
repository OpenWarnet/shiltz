#pragma once

#include <cstdint>

// A drop appeared on its map (see Map::Spawn(Drop)).
struct DropAddEvent
{
    std::uint32_t id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t item_id = 0;
};

// A drop was taken off its map (see Map::Despawn(const Drop&)); it is already gone when this is
// dispatched, so x/y are carried here rather than looked up again.
struct DropRemoveEvent
{
    std::uint32_t id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
};
