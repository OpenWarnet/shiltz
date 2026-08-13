#include "ItemTable.h"

void ItemTable::Load(const std::filesystem::path& dir)
{
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".scr")
            continue;

        for (auto& record : ItemScr::Load(entry.path()))
        {
            const std::int64_t id = record.id;
            m_records.emplace(id, std::move(record));
        }
    }
}

const ItemRecord* ItemTable::Find(std::int64_t itemId) const
{
    auto it = m_records.find(itemId);
    return it != m_records.end() ? &it->second : nullptr;
}
