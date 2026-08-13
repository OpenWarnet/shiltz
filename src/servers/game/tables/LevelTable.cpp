#include "LevelTable.h"

void LevelTable::Load(const std::filesystem::path& path)
{
    for (auto& record : LevelScr::Load(path))
    {
        const std::int64_t level = record.level;
        m_records.emplace(level, std::move(record));
    }
}

const LevelRecord* LevelTable::Find(std::int64_t level) const
{
    auto it = m_records.find(level);
    return it != m_records.end() ? &it->second : nullptr;
}
