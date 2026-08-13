#include "StatusTable.h"

namespace
{
    // Packs a (classId, blockId) pair into one collision-free int64 key --
    // classId in the high 32 bits, blockId in the low 32 bits.
    std::int64_t MakeKey(std::int64_t classId, std::int64_t blockId)
    {
        return (classId << 32) | (blockId & 0xFFFFFFFFLL);
    }
} // namespace

void StatusTable::Load(const std::filesystem::path& path)
{
    for (auto& record : StatusScr::Load(path))
    {
        const std::int64_t key = MakeKey(record.class_id, record.block_id);
        m_rates.emplace(key, record.value);
    }
}

const double* StatusTable::Find(std::size_t block, JobId jobId) const
{
    auto it = m_rates.find(MakeKey(static_cast<std::int64_t>(jobId), static_cast<std::int64_t>(block)));
    return it != m_rates.end() ? &it->second : nullptr;
}
