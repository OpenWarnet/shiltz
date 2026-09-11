#include "Atlas.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

Atlas::Atlas(std::size_t maxMapId)
    : m_maps(maxMapId + 1)
{
}

void Atlas::Add(MapRecord record, const MonsterTable& monsters)
{
    const auto mapId = record.server_map_id;
    if (mapId <= 0)
        return;

    if (mapId >= static_cast<std::int64_t>(m_maps.size()))
    {
        throw std::out_of_range("server_map_id " + std::to_string(mapId) +
                                " exceeds Atlas maximum of " +
                                std::to_string(m_maps.size() - 1));
    }

    auto& slot = m_maps[static_cast<std::size_t>(mapId)];
    if (slot)
        throw std::runtime_error("duplicate server_map_id " + std::to_string(mapId));

    try
    {
        slot = std::make_unique<Map>(std::move(record), monsters);
        ++m_mapCount;
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("server_map_id " + std::to_string(mapId) + ": " + e.what());
    }
}

Map* Atlas::Get(std::int64_t mapId)
{
    if (mapId <= 0 || mapId >= static_cast<std::int64_t>(m_maps.size()))
        return nullptr;

    return m_maps[static_cast<std::size_t>(mapId)].get();
}

const Map* Atlas::Get(std::int64_t mapId) const
{
    if (mapId <= 0 || mapId >= static_cast<std::int64_t>(m_maps.size()))
        return nullptr;

    return m_maps[static_cast<std::size_t>(mapId)].get();
}

void Atlas::Tick(std::chrono::milliseconds delta)
{
    if (Empty())
        return;

    for (std::size_t mapId = 1; mapId < m_maps.size(); ++mapId)
    {
        Map* map = m_maps[mapId].get();
        if (map == nullptr)
            continue;

        try
        {
            map->Tick(delta);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Map::Tick threw for server_map_id " << mapId << ": " << e.what()
                      << "\n";
        }
    }
}

std::size_t Atlas::Size() const noexcept
{
    return m_mapCount;
}

bool Atlas::Empty() const noexcept
{
    return m_mapCount == 0;
}
