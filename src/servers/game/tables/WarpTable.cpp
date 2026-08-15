#include "WarpTable.h"

void WarpTable::Load(const std::filesystem::path& path)
{
    for (auto& record : WarpScr::Load(path))
    {
        const std::int64_t warpId = record.warp_id;
        m_records.emplace(warpId, std::move(record));
    }
}

const WarpRecord* WarpTable::Find(std::int64_t warpId) const
{
    auto it = m_records.find(warpId);
    return it != m_records.end() ? &it->second : nullptr;
}
