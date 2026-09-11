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

// Signed so a -1 step at coordinate 0 clamps instead of wrapping.
std::uint32_t StepCoordinate(std::uint32_t coordinate, std::int32_t delta,
                             std::uint32_t maxCoordinate)
{
    return static_cast<std::uint32_t>(
        std::clamp<std::int64_t>(std::int64_t{coordinate} + delta, 0, maxCoordinate));
}

void RollNextAiState(Creature& creature, std::uint32_t maxCoordinate)
{
    const std::uint64_t roll = MixDecisionSeed(creature.instance_id, creature.ai_decision_seq++);

    if (roll & 1)
    {
        const auto [dx, dy] = kWanderOffsets[(roll >> 1) % kWanderOffsets.size()];
        creature.x = StepCoordinate(creature.x, dx, maxCoordinate);
        creature.y = StepCoordinate(creature.y, dy, maxCoordinate);

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

std::size_t Zone::Coordinates::Index() const noexcept
{
    return static_cast<std::size_t>(y) * Map::kZoneGridSize + static_cast<std::size_t>(x);
}

Zone::Coordinates Zone::Of(std::uint32_t x, std::uint32_t y) noexcept
{
    return {static_cast<std::int32_t>(x / kSize), static_cast<std::int32_t>(y / kSize)};
}

bool Zone::IsInGrid(Coordinates zone) noexcept
{
    constexpr auto kLimit = static_cast<std::int32_t>(Map::kZoneGridSize);
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

void Zone::AddCreature(Creature creature)
{
    m_creatures.push_back(std::move(creature));
}

std::span<const Creature> Zone::Creatures() const noexcept
{
    return m_creatures;
}

Zone::TickResult Zone::Tick(std::chrono::milliseconds delta, std::uint32_t maxCoordinate)
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

        const std::uint32_t fromX = creature.x;
        const std::uint32_t fromY = creature.y;

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

bool Zone::Contains(std::uint32_t x, std::uint32_t y) const noexcept
{
    return Of(x, y) == Coordinates{m_x, m_y};
}
