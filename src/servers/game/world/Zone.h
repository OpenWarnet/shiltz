#pragma once

#include "Grid.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// Geometry helpers for the Map's 16x16 view grid. Creature ownership and
// simulation remain entirely in Map.
class Zone
{
public:
    struct Coordinates
    {
        std::int32_t x = 0;
        std::int32_t y = 0;

        // Row-major slot in the map's zone grid; only meaningful when IsInGrid.
        std::size_t Index() const noexcept;

        bool operator==(const Coordinates&) const = default;
    };

    static constexpr std::uint32_t kSize = 16;
    static constexpr std::uint32_t kGridSize = Grid::kSize / kSize;
    static constexpr std::size_t kCount =
        static_cast<std::size_t>(kGridSize) * kGridSize;

    static Coordinates Of(std::uint32_t x, std::uint32_t y) noexcept;

    // True if `zone` lies inside the map's zone grid.
    static bool IsInGrid(Coordinates zone) noexcept;

    // True if moving from (fromX, fromY) to (toX, toY) lands in a different zone.
    static bool Crossed(std::uint32_t fromX, std::uint32_t fromY, std::uint32_t toX,
                        std::uint32_t toY) noexcept;

    // The 3x3 block of zones centered on `zone`, clipped to the map's zone grid.
    static std::vector<Coordinates> Around(Coordinates zone);

    // True if the zones touch, diagonally included, or are the same zone.
    static bool IsNeighboring(Coordinates a, Coordinates b) noexcept;
};
