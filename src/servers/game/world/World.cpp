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

    std::filesystem::path ItemDataDir()
    {
        return DataDir() / "item";
    }
} // namespace

void World::Start()
{
    std::cout << "World started\n";

    for (auto& record : MonsterScr::Load(DataDir() / "monster.scr"))
    {
        const std::int64_t id = record.id;
        m_monsterRecords.emplace(id, std::move(record));
    }

    m_map = MapLoader::Load(MapDataDir() / "npc32.scr", MapDataDir() / "m32.scr",
                             [this] { return AllocateCreatureInstanceId(); });

    for (auto& record : SellerScr::Load(DataDir() / "seller.scr"))
    {
        const std::int64_t shopId = record.shop_id;
        m_sellerRecords.emplace(shopId, std::move(record));
    }

    // itemNN.scr is split across many files (item.scr, item1.scr, ...,
    // item31.scr) rather than one table -- load every ".scr" file in the
    // directory instead of naming each one, so a new file dropped in later
    // is picked up without a code change.
    for (const auto& entry : std::filesystem::directory_iterator(ItemDataDir()))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".scr")
            continue;

        for (auto& record : ItemScr::Load(entry.path()))
        {
            const std::int64_t id = record.id;
            m_itemRecords.emplace(id, std::move(record));
        }
    }
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

const MonsterRecord* World::FindMonsterRecord(std::int64_t monsterId) const
{
    auto it = m_monsterRecords.find(monsterId);
    return it != m_monsterRecords.end() ? &it->second : nullptr;
}

const SellerRecord* World::FindSellerRecord(std::int64_t shopId) const
{
    auto it = m_sellerRecords.find(shopId);
    return it != m_sellerRecords.end() ? &it->second : nullptr;
}

const ItemRecord* World::FindItemRecord(std::int64_t itemId) const
{
    auto it = m_itemRecords.find(itemId);
    return it != m_itemRecords.end() ? &it->second : nullptr;
}
