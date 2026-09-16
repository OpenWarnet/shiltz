#pragma once

#include <cstdint>

// A living monster took one world-simulation step.
struct CreatureMoveEvent
{
    std::uint32_t creature_id = 0;
    std::uint32_t from_x = 0;
    std::uint32_t from_y = 0;
    std::uint32_t to_x = 0;
    std::uint32_t to_y = 0;
    std::uint32_t movement_mode = 1; // 1 in every observed wander packet
};

// A dead monster returned at its spawn using the same runtime instance id.
struct CreatureRespawnEvent
{
    std::uint32_t creature_id = 0;
};
