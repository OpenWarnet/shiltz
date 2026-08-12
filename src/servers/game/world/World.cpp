#include "World.h"

#include "MapLoader.h"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <string_view>

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

    std::filesystem::path SkillDataDir()
    {
        return DataDir() / "skill";
    }

    // Packs a (skillId, level) pair into one collision-free int64 key --
    // skillId in the high 32 bits, level in the low 32 bits, so any two
    // distinct pairs always land on distinct keys regardless of either
    // value's magnitude (unlike e.g. skillId * 100 + level, which silently
    // collides if level ever reaches double digits... which it already
    // does, up to skill23.scr).
    std::int64_t MakeSkillLevelKey(std::int64_t skillId, std::int64_t level)
    {
        return (skillId << 32) | (level & 0xFFFFFFFFLL);
    }

    // Same idiom, for status.scr's (classId, blockId) pairs.
    std::int64_t MakeStatusKey(std::int64_t classId, std::int64_t blockId)
    {
        return (classId << 32) | (blockId & 0xFFFFFFFFLL);
    }

    // skillNN.scr's level isn't a column in the row -- it's which file the
    // row came from. Parses "skillNN" -> NN; returns 0 (an invalid level,
    // never matched by MakeSkillLevelKey's callers) for anything that
    // doesn't fit that shape, e.g. a future uskillNN.scr dropped in the
    // same directory.
    std::int64_t ParseSkillFileLevel(const std::filesystem::path& path)
    {
        static constexpr std::string_view kPrefix = "skill";

        const std::string stem = path.stem().string();
        if (stem.size() <= kPrefix.size() || stem.compare(0, kPrefix.size(), kPrefix) != 0)
            return 0;

        std::int64_t level = 0;
        const std::string digits = stem.substr(kPrefix.size());
        auto result = std::from_chars(digits.data(), digits.data() + digits.size(), level);

        return result.ec == std::errc() ? level : 0;
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

    for (auto& record : LevelScr::Load(DataDir() / "level.scr"))
    {
        const std::int64_t level = record.level;
        m_levelRecords.emplace(level, std::move(record));
    }

    for (auto& record : StatusScr::Load(DataDir() / "status.scr"))
    {
        const std::int64_t key = MakeStatusKey(record.class_id, record.block_id);
        m_statusRates.emplace(key, record.value);
    }

    // skillNN.scr is split one file per skill level (skill01.scr = every
    // skill's attributes at level 1, ...), same "load every .scr file in
    // the directory" approach as ItemDataDir -- the level comes from the
    // filename (ParseSkillFileLevel), not a column in the row.
    for (const auto& entry : std::filesystem::directory_iterator(SkillDataDir()))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".scr")
            continue;

        const std::int64_t level = ParseSkillFileLevel(entry.path());
        if (level <= 0)
            continue;

        for (auto& record : SkillScr::Load(entry.path()))
        {
            const std::int64_t key = MakeSkillLevelKey(record.id, level);
            m_skillRecords.emplace(key, std::move(record));
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

const LevelRecord* World::FindLevelRecord(std::int64_t level) const
{
    auto it = m_levelRecords.find(level);
    return it != m_levelRecords.end() ? &it->second : nullptr;
}

const SkillRecord* World::FindSkillRecord(std::int64_t skillId, std::int64_t level) const
{
    auto it = m_skillRecords.find(MakeSkillLevelKey(skillId, level));
    return it != m_skillRecords.end() ? &it->second : nullptr;
}

const double* World::FindStatusRate(std::size_t block, JobId jobId) const
{
    auto it = m_statusRates.find(MakeStatusKey(static_cast<std::int64_t>(jobId),
                                                static_cast<std::int64_t>(block)));
    return it != m_statusRates.end() ? &it->second : nullptr;
}
