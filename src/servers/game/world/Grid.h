#pragma once

#include <algorithm>
#include <cstdint>

// The fixed tile grid every Map is laid out on. map.scr carries no per-map
// dimensions, so the bound is a world constant rather than something a Map
// owns and hands out.
namespace Grid
{
inline constexpr std::uint32_t kSize = 512;
inline constexpr std::uint32_t kMaxCoordinate = kSize - 1;

// True if tile (x, y) is on the grid; anything else must be rejected before it reaches the world.
[[nodiscard]] inline constexpr bool Contains(std::uint32_t x, std::uint32_t y) noexcept
{
    return x < kSize && y < kSize;
}

// Moves one coordinate by delta, clamped to the grid.
[[nodiscard]] inline constexpr std::uint32_t Step(std::uint32_t coordinate,
                                                  std::int32_t delta) noexcept
{
    return static_cast<std::uint32_t>(
        std::clamp<std::int64_t>(std::int64_t{coordinate} + delta, 0, kMaxCoordinate));
}
} // namespace Grid
