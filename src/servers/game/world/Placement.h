#pragma once

#include <cstdint>

enum class Direction : std::uint32_t
{
    None = 0,
    North = 1,
    NorthWest = 2,
    West = 3,
    SouthWest = 4,
    South = 5,
    SouthEast = 6,
    East = 7,
    NorthEast = 8,
};

// An entity's current location and facing within one Map. The Map itself is
// intentionally not repeated here because it already owns every placed entity.
struct Placement
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    Direction direction = Direction::None;

    // Chebyshev distance: orthogonal and diagonal neighbours are both one
    // tile away, matching the game's eight-way movement and facing model.
    [[nodiscard]] std::uint32_t DistanceTo(const Placement& other) const noexcept;
    [[nodiscard]] bool IsWithinRange(const Placement& other, std::uint32_t range) const noexcept;
    [[nodiscard]] Direction DirectionTo(const Placement& other) const noexcept;
    [[nodiscard]] bool IsFacing(const Placement& other) const noexcept;
    [[nodiscard]] bool IsAt(const Placement& other) const noexcept;
};
