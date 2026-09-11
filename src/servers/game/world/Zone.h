#pragma once

#include "Creature.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

class Map;

// One 16x16 spatial bucket within a Map; Zone owns the creatures currently located in it.
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

    static constexpr std::int32_t kSize = 16;

    struct CreatureMove
    {
        std::uint32_t creature_id = 0;
        std::uint32_t from_x = 0;
        std::uint32_t from_y = 0;
        std::uint32_t to_x = 0;
        std::uint32_t to_y = 0;
    };

    Zone(std::int32_t x, std::int32_t y) noexcept;

    std::int32_t X() const noexcept;
    std::int32_t Y() const noexcept;

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

private:
    friend class Map;

    struct TickResult
    {
        std::vector<CreatureMove> moves;
        std::vector<Creature> relocated;
    };

    void AddCreature(Creature creature);
    std::span<const Creature> Creatures() const noexcept;
    TickResult Tick(std::chrono::milliseconds delta, std::int32_t maxCoordinate);
    bool Contains(std::uint32_t x, std::uint32_t y) const noexcept;

    std::int32_t m_x;
    std::int32_t m_y;
    std::vector<Creature> m_creatures;
};
