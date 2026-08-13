#include "MonsterTable.h"

void MonsterTable::Load(const std::filesystem::path& path)
{
    for (auto& record : MonsterScr::Load(path))
    {
        const std::int64_t id = record.id;
        m_records.emplace(id, std::move(record));
    }
}

const MonsterRecord* MonsterTable::Find(std::int64_t monsterId) const
{
    auto it = m_records.find(monsterId);
    return it != m_records.end() ? &it->second : nullptr;
}
