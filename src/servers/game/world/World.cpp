#include "World.h"

#include "world/common/Paths.h"
#include "parser/MapScr.h"

#include <boost/asio/post.hpp>
#include <filesystem>
#include <iostream>
#include <latch>
#include <stdexcept>

void World::Start()
{
    std::cout << "World started\n";

    for (const auto& record : MapScr::Load(Paths::Data.root / "map.scr"))
    {
        if (record.server_map_id <= 0)
            continue;

        try
        {
            Map map(record, [this] { return AllocateCreatureInstanceId(); });
            m_maps.emplace(map.id, std::move(map));
        }
        catch (const std::runtime_error& e)
        {
            std::cout << "Skipping map server_map_id " << record.server_map_id << ": " << e.what()
                      << "\n";
        }
    }
}

void World::Shutdown()
{
    std::cout << "World shut down\n";
}

Map* World::GetMap(std::int64_t serverMapId)
{
    auto it = m_maps.find(serverMapId);
    return it != m_maps.end() ? &it->second : nullptr;
}

const Map* World::GetMap(std::int64_t serverMapId) const
{
    auto it = m_maps.find(serverMapId);
    return it != m_maps.end() ? &it->second : nullptr;
}

std::uint32_t World::AllocateCreatureInstanceId()
{
    return m_nextCreatureInstanceId++;
}

std::vector<MapTickResult> World::Tick(std::chrono::milliseconds delta)
{
    if (m_maps.empty())
        return {};

    std::vector<MapTickResult> results(m_maps.size());
    std::latch remaining(static_cast<std::ptrdiff_t>(m_maps.size()));

    std::size_t i = 0;
    for (auto& [serverMapId, map] : m_maps)
    {
        MapTickResult& result = results[i++];
        result.server_map_id = serverMapId;

        // Only maps with someone actually on them are worth simulating this
        // tick -- an empty map's creatures just have their ai_timer left
        // as-is (see the comment on Map::Tick) rather than being wandered
        // around for nobody to see. Counting the latch down inline (instead
        // of posting a no-op task) still leaves exactly m_maps.size()
        // count_downs total, matching the latch's initial count above.
        if (!map.HasPlayers())
        {
            remaining.count_down();
            continue;
        }
    }

    // Block the caller (GameServer's world strand) until every map posted
    // above has finished -- this is the only synchronization point between
    // maps; nothing else couples their ticks together.
    remaining.wait();

    return results;
}
