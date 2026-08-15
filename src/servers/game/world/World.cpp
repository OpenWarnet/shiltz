#include "World.h"

#include "MapLoader.h"
#include "parser/MapScr.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    std::filesystem::path DataDir()
    {
        return std::filesystem::path(SHILTZ_SOURCE_DIR) / "src" / "servers" / "game" / "world" /
               "data";
    }

    std::filesystem::path MonsterSpawnDataDir()
    {
        return DataDir() / "spawn" / "monster";
    }

    std::filesystem::path NpcSpawnDataDir()
    {
        return DataDir() / "spawn" / "npc";
    }
} // namespace

void World::Start()
{
    std::cout << "World started\n";

    for (const auto& record : MapScr::Load(DataDir() / "map.scr"))
    {
        // -1 (and any other non-positive value) marks an unused map.scr
        // slot -- every other field on those rows is blank/zero too, so
        // there's no monster_file/npc_file to load.
        if (record.server_map_id <= 0)
            continue;

        // Some map.scr rows reference an npc/monster spawn file that isn't
        // actually present under data/spawn/{npc,monster} (e.g. server_map_id
        // 568-574 as of this writing) -- skip that one map rather than
        // taking down World::Start() over a data gap in an otherwise-valid
        // row.
        try
        {
            Map map = MapLoader::Load(NpcSpawnDataDir() / (record.npc_file + ".scr"),
                                       MonsterSpawnDataDir() / (record.monster_file + ".scr"),
                                       [this] { return AllocateCreatureInstanceId(); });
            m_maps.emplace(record.server_map_id, std::move(map));
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

void World::Tick(std::chrono::milliseconds delta)
{
    for (auto& [serverMapId, map] : m_maps)
        map.Tick(delta);
}
