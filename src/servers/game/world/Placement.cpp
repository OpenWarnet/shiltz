#include "Placement.h"

#include <algorithm>
#include <cstdint>

namespace
{
constexpr std::uint32_t AxisDistance(std::uint32_t from, std::uint32_t to) noexcept
{
    return from > to ? from - to : to - from;
}

constexpr std::int32_t DirectionDelta(std::uint32_t from, std::uint32_t to) noexcept
{
    return to < from ? -1 : to > from ? 1 : 0;
}

constexpr Direction DirectionFromDelta(std::int32_t dx, std::int32_t dy) noexcept
{
    // Rows are north/current/south; columns are west/current/east.
    constexpr Direction directions[3][3] = {
        {Direction::NorthWest, Direction::North, Direction::NorthEast},
        {Direction::West, Direction::None, Direction::East},
        {Direction::SouthWest, Direction::South, Direction::SouthEast},
    };

    return directions[dy + 1][dx + 1];
}

static_assert(DirectionFromDelta(0, -1) == Direction::North);
static_assert(DirectionFromDelta(-1, -1) == Direction::NorthWest);
static_assert(DirectionFromDelta(-1, 0) == Direction::West);
static_assert(DirectionFromDelta(-1, 1) == Direction::SouthWest);
static_assert(DirectionFromDelta(0, 1) == Direction::South);
static_assert(DirectionFromDelta(1, 1) == Direction::SouthEast);
static_assert(DirectionFromDelta(1, 0) == Direction::East);
static_assert(DirectionFromDelta(1, -1) == Direction::NorthEast);
} // namespace

std::uint32_t Placement::DistanceTo(const Placement& other) const noexcept
{
    return std::max(AxisDistance(x, other.x), AxisDistance(y, other.y));
}

bool Placement::IsWithinRange(const Placement& other, std::uint32_t range) const noexcept
{
    return DistanceTo(other) <= range;
}

Direction Placement::DirectionTo(const Placement& other) const noexcept
{
    return DirectionFromDelta(DirectionDelta(x, other.x), DirectionDelta(y, other.y));
}

bool Placement::IsFacing(const Placement& other) const noexcept
{
    return direction != Direction::None && direction == DirectionTo(other);
}

bool Placement::IsAt(const Placement& other) const noexcept
{
    return x == other.x && y == other.y;
}
