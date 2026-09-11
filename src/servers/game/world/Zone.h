#pragma once

#include "Creature.h"

#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

class Map;

// One 16x16 spatial bucket within a Map. Map owns the grid and synchronizes
// access to it; Zone owns the creatures currently located in this bucket.
class Zone
{
public:
    using Coordinates = std::pair<std::int32_t, std::int32_t>;

    static constexpr std::int32_t kSize = 16;

    struct CreatureMove
    {
        std::uint32_t creature_id = 0;
        std::int32_t from_x = 0;
        std::int32_t from_y = 0;
        std::int32_t to_x = 0;
        std::int32_t to_y = 0;
    };

    Zone(std::int32_t x, std::int32_t y) noexcept;

    std::int32_t X() const noexcept;
    std::int32_t Y() const noexcept;

    static Coordinates Of(std::int32_t x, std::int32_t y) noexcept;

    // True if moving from (fromX, fromY) to (toX, toY) lands in a different zone.
    static bool Crossed(std::int32_t fromX, std::int32_t fromY, std::int32_t toX,
                        std::int32_t toY) noexcept;

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
    std::vector<Creature> CreatureSnapshot() const;
    TickResult Tick(std::chrono::milliseconds delta, std::int32_t maxCoordinate);
    bool Contains(std::int32_t x, std::int32_t y) const noexcept;

    std::int32_t m_x;
    std::int32_t m_y;
    std::vector<Creature> m_creatures;
};
