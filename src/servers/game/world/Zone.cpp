#include "Zone.h"

#include <cstdlib>

std::size_t Zone::Coordinates::Index() const noexcept
{
    return static_cast<std::size_t>(y) * Zone::kGridSize + static_cast<std::size_t>(x);
}

Zone::Coordinates Zone::Of(std::uint32_t x, std::uint32_t y) noexcept
{
    return {static_cast<std::int32_t>(x / kSize), static_cast<std::int32_t>(y / kSize)};
}

bool Zone::IsInGrid(Coordinates zone) noexcept
{
    constexpr auto kLimit = static_cast<std::int32_t>(kGridSize);
    return zone.x >= 0 && zone.x < kLimit && zone.y >= 0 && zone.y < kLimit;
}

bool Zone::Crossed(std::uint32_t fromX, std::uint32_t fromY, std::uint32_t toX,
                   std::uint32_t toY) noexcept
{
    return Of(fromX, fromY) != Of(toX, toY);
}

std::vector<Zone::Coordinates> Zone::Around(Coordinates zone)
{
    std::vector<Coordinates> zones;
    zones.reserve(9);

    for (std::int32_t dy = -1; dy <= 1; ++dy)
    {
        for (std::int32_t dx = -1; dx <= 1; ++dx)
        {
            const Coordinates neighbor{zone.x + dx, zone.y + dy};
            if (IsInGrid(neighbor))
                zones.push_back(neighbor);
        }
    }

    return zones;
}

bool Zone::IsNeighboring(Coordinates a, Coordinates b) noexcept
{
    return std::abs(a.x - b.x) <= 1 && std::abs(a.y - b.y) <= 1;
}
