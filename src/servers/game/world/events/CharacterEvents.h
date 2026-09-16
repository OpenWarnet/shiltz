#pragma once

#include "../Zone.h"

#include <cstdint>
#include <optional>

// A character was spawned on its map (see Map::Spawn).
struct CharacterJoinEvent
{
    std::uint32_t instance_id = 0;
};

// A character was taken off its map (see Map::Despawn); it is already gone when this is dispatched.
struct CharacterLeaveEvent
{
    std::uint32_t instance_id = 0;
};

// A character walked on its map, as sent in CG_MOVE; `from` is kept because
// character.placement already holds `to` at dispatch.
struct CharacterMoveEvent
{
    std::uint32_t instance_id = 0;
    std::uint32_t from_x = 0;
    std::uint32_t from_y = 0;
    std::uint32_t to_x = 0;
    std::uint32_t to_y = 0;
    std::uint32_t direction = 0;
    std::uint32_t speed = 0;
    std::uint32_t stop_direction = 0;
};

// A character's 3x3 view moved to another zone; `from` is empty when it was just placed on the map.
struct CharacterZoneChangeEvent
{
    std::uint32_t instance_id = 0;
    std::optional<Zone::Coordinates> from;
    Zone::Coordinates to;
};
