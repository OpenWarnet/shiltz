#include "AiMonTable.h"

void AiMonTable::Load(const std::filesystem::path& path)
{
    for (auto& record : AiMonScr::Load(path))
    {
        const std::int64_t id = record.id;
        m_records.emplace(id, std::move(record));
    }
}

const AiMonRecord* AiMonTable::Find(std::int64_t id) const
{
    auto it = m_records.find(id);
    return it != m_records.end() ? &it->second : nullptr;
}
