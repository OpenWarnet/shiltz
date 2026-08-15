#include "Map.h"

#include <algorithm>
#include <iostream>

namespace
{
    std::pair<std::int32_t, std::int32_t> ZoneOf(std::int32_t x, std::int32_t y)
    {
        return {x / Map::kZoneSize, y / Map::kZoneSize};
    }

    bool InZoneGrid(std::int32_t zoneX, std::int32_t zoneY)
    {
        return zoneX >= 0 && zoneX < Map::kZoneGridSize && zoneY >= 0 && zoneY < Map::kZoneGridSize;
    }

    std::size_t ZoneIndex(std::int32_t zoneX, std::int32_t zoneY)
    {
        return static_cast<std::size_t>(zoneY) * static_cast<std::size_t>(Map::kZoneGridSize) +
               static_cast<std::size_t>(zoneX);
    }
} // namespace

Map::Map(Map&& other)
{
    std::lock_guard lock(other.m_itemsMutex);
    m_items = std::move(other.m_items);
    m_creatureGrid = std::move(other.m_creatureGrid);
}

Map& Map::operator=(Map&& other)
{
    if (this != &other)
    {
        std::scoped_lock lock(m_itemsMutex, other.m_itemsMutex);
        m_items = std::move(other.m_items);
        m_creatureGrid = std::move(other.m_creatureGrid);
    }
    return *this;
}

void Map::AddItem(GroundItem item)
{
    std::lock_guard lock(m_itemsMutex);
    m_items.push_back(item);
}

bool Map::RemoveItem(std::uint32_t id)
{
    std::lock_guard lock(m_itemsMutex);
    auto it = std::find_if(m_items.begin(), m_items.end(),
                            [id](const GroundItem& item) { return item.id == id; });
    if (it == m_items.end())
        return false;

    m_items.erase(it);
    return true;
}

std::optional<GroundItem> Map::TryTakeItem(std::uint32_t id)
{
    std::lock_guard lock(m_itemsMutex);
    auto it = std::find_if(m_items.begin(), m_items.end(),
                            [id](const GroundItem& item) { return item.id == id; });
    if (it == m_items.end())
        return std::nullopt;

    GroundItem taken = *it;
    m_items.erase(it);
    return taken;
}

std::vector<GroundItem> Map::Items() const
{
    std::lock_guard lock(m_itemsMutex);
    return m_items;
}

void Map::AddCreature(Creature creature)
{
    const auto [zoneX, zoneY] = ZoneOf(creature.x, creature.y);
    if (!InZoneGrid(zoneX, zoneY))
    {
        std::cout << "Ignoring creature " << creature.monster_id
                  << " with out-of-range position (" << creature.x << ", " << creature.y << ")\n";
        return;
    }

    m_creatureGrid[ZoneIndex(zoneX, zoneY)].push_back(creature);
}

const std::vector<Creature>& Map::CreaturesInZone(std::int32_t zoneX, std::int32_t zoneY) const
{
    static const std::vector<Creature> kEmpty;
    if (!InZoneGrid(zoneX, zoneY))
        return kEmpty;

    return m_creatureGrid[ZoneIndex(zoneX, zoneY)];
}

void Map::Tick(std::chrono::milliseconds /*delta*/)
{
    // Nothing to simulate yet -- extension point for respawns, regen, AI.
}

std::vector<std::pair<std::int32_t, std::int32_t>> Map::ZonesAround(std::int32_t x, std::int32_t y) const
{
    std::vector<std::pair<std::int32_t, std::int32_t>> zones;
    const auto [zoneX, zoneY] = ZoneOf(x, y);

    for (std::int32_t dy = -1; dy <= 1; ++dy)
    {
        for (std::int32_t dx = -1; dx <= 1; ++dx)
        {
            const std::int32_t neighborX = zoneX + dx;
            const std::int32_t neighborY = zoneY + dy;
            if (InZoneGrid(neighborX, neighborY))
                zones.emplace_back(neighborX, neighborY);
        }
    }

    return zones;
}
