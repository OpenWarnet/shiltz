#include "World.h"

#include "MapLoader.h"

#include <filesystem>
#include <iostream>

namespace
{
    std::filesystem::path DataDir()
    {
        return std::filesystem::path(SHILTZ_SOURCE_DIR) / "src" / "servers" / "game" / "world" /
               "data";
    }

    // Only one map's spawn data exists today (world/data/map/npc32.scr +
    // m32.scr), so it's loaded by name here rather than through a
    // map_id-keyed registry -- see MapLoader.h and Map.h for the shape
    // this feeds into. Revisit once a second map's data shows up.
    std::filesystem::path MapDataDir()
    {
        return DataDir() / "map";
    }
} // namespace

void World::Start()
{
    std::cout << "World started\n";

    m_map = MapLoader::Load(MapDataDir() / "npc32.scr", MapDataDir() / "m32.scr",
                             [this] { return AllocateCreatureInstanceId(); });
}

void World::Shutdown()
{
    std::cout << "World shut down\n";
}

Map& World::GetMap()
{
    return m_map;
}

const Map& World::GetMap() const
{
    return m_map;
}

std::uint32_t World::AllocateCreatureInstanceId()
{
    return m_nextCreatureInstanceId++;
}
