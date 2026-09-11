#pragma once

#include <cstdint>
#include <optional>

// A character was spawned on its map (see Map::Spawn).
struct CharacterJoinEvent
{
    std::uint32_t instance_id = 0;
};

// How a character walked, as sent in CG_MOVE.
struct CharacterWalk
{
    std::uint32_t direction = 0;
    std::uint32_t speed = 0;
    std::uint32_t stop_direction = 0;
};

// A character moved on its map; `from` is kept because character.x/y already holds `to` at dispatch.
struct CharacterMoveEvent
{
    std::uint32_t instance_id = 0;
    std::int32_t from_x = 0;
    std::int32_t from_y = 0;
    std::int32_t to_x = 0;
    std::int32_t to_y = 0;

    // Empty when the character was placed (enter, warp) rather than walked.
    std::optional<CharacterWalk> walk;
};
