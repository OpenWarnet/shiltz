#include "SetOptionTable.h"

namespace
{
    // Packs a (setId, pieceCount) pair into one collision-free int64 key --
    // setId in the high 32 bits, pieceCount in the low 32 bits.
    std::int64_t MakeKey(std::int64_t setId, std::int64_t pieceCount)
    {
        return (setId << 32) | (pieceCount & 0xFFFFFFFFLL);
    }
} // namespace

void SetOptionTable::Load(const std::filesystem::path& path)
{
    for (auto& record : SetOptScr::Load(path))
    {
        const std::int64_t key = MakeKey(record.set_id, record.piece_count);
        m_records.emplace(key, std::move(record));
    }
}

const SetOptionRecord* SetOptionTable::Find(std::int64_t setId, std::int64_t pieceCount) const
{
    auto it = m_records.find(MakeKey(setId, pieceCount));
    return it != m_records.end() ? &it->second : nullptr;
}
