#include "Zone.h"

#include "Map.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iterator>

namespace
{
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds kIdleBaseDelay = 2000ms;
constexpr std::chrono::milliseconds kIdleMaxJitter = 1000ms;
constexpr std::chrono::milliseconds kWanderBaseDelay = 5000ms;
constexpr std::chrono::milliseconds kWanderMaxJitter = 1000ms;

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8> kWanderOffsets{{
    {-1, -1},
    {0, -1},
    {1, -1},
    {-1, 0},
    {1, 0},
    {-1, 1},
    {0, 1},
    {1, 1},
}};

std::uint64_t MixDecisionSeed(std::uint32_t instanceId, std::uint32_t decisionSeq)
{
    std::uint64_t x = (static_cast<std::uint64_t>(instanceId) << 32) | decisionSeq;
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

void RollNextAiState(Creature& creature, std::int32_t maxCoordinate)
{
    const std::uint64_t roll = MixDecisionSeed(creature.instance_id, creature.ai_decision_seq++);

    if (roll & 1)
    {
        const auto [dx, dy] = kWanderOffsets[(roll >> 1) % kWanderOffsets.size()];
        creature.x = std::clamp(creature.x + dx, 0, maxCoordinate);
        creature.y = std::clamp(creature.y + dy, 0, maxCoordinate);

        creature.ai_state = CreatureAiState::Wander;
        creature.ai_timer =
            kWanderBaseDelay + std::chrono::milliseconds((roll >> 4) % kWanderMaxJitter.count());
    }
    else
    {
        creature.ai_state = CreatureAiState::Idle;
        creature.ai_timer =
            kIdleBaseDelay + std::chrono::milliseconds((roll >> 4) % kIdleMaxJitter.count());
    }
}
} // namespace

Zone::Zone(std::int32_t x, std::int32_t y) noexcept : m_x(x), m_y(y)
{
}

std::int32_t Zone::X() const noexcept
{
    return m_x;
}

std::int32_t Zone::Y() const noexcept
{
    return m_y;
}

Zone::Coordinates Zone::Of(std::int32_t x, std::int32_t y) noexcept
{
    return {x / kSize, y / kSize};
}

bool Zone::Crossed(std::int32_t fromX, std::int32_t fromY, std::int32_t toX,
                   std::int32_t toY) noexcept
{
    return Of(fromX, fromY) != Of(toX, toY);
}

std::vector<Zone::Coordinates> Zone::Around(Coordinates zone)
{
    std::vector<Coordinates> zones;
    const auto [zoneX, zoneY] = zone;

    for (std::int32_t dy = -1; dy <= 1; ++dy)
    {
        for (std::int32_t dx = -1; dx <= 1; ++dx)
        {
            const std::int32_t neighborX = zoneX + dx;
            const std::int32_t neighborY = zoneY + dy;
            if (neighborX >= 0 && neighborX < Map::kZoneGridSize && neighborY >= 0 &&
                neighborY < Map::kZoneGridSize)
                zones.emplace_back(neighborX, neighborY);
        }
    }

    return zones;
}

bool Zone::IsNeighboring(Coordinates a, Coordinates b) noexcept
{
    return std::abs(a.first - b.first) <= 1 && std::abs(a.second - b.second) <= 1;
}

void Zone::AddCreature(Creature creature)
{
    m_creatures.push_back(std::move(creature));
}

std::vector<Creature> Zone::CreatureSnapshot() const
{
    return m_creatures;
}

Zone::TickResult Zone::Tick(std::chrono::milliseconds delta, std::int32_t maxCoordinate)
{
    TickResult result;

    for (Creature& creature : m_creatures)
    {
        if (creature.kind != CreatureKind::Monster)
            continue;

        if (creature.ai_timer > delta)
        {
            creature.ai_timer -= delta;
            continue;
        }

        const std::int32_t fromX = creature.x;
        const std::int32_t fromY = creature.y;

        RollNextAiState(creature, maxCoordinate);

        if (creature.x != fromX || creature.y != fromY)
        {
            result.moves.push_back(CreatureMove{
                .creature_id = creature.instance_id,
                .from_x = fromX,
                .from_y = fromY,
                .to_x = creature.x,
                .to_y = creature.y,
            });
        }
    }

    const auto stillBelongsHere = [this](const Creature& creature)
    { return Contains(creature.x, creature.y); };
    const auto misplaced =
        std::partition(m_creatures.begin(), m_creatures.end(), stillBelongsHere);
    result.relocated.insert(result.relocated.end(), std::make_move_iterator(misplaced),
                            std::make_move_iterator(m_creatures.end()));
    m_creatures.erase(misplaced, m_creatures.end());

    return result;
}

bool Zone::Contains(std::int32_t x, std::int32_t y) const noexcept
{
    return Of(x, y) == Coordinates{m_x, m_y};
}
