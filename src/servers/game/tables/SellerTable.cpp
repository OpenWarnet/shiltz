#include "SellerTable.h"

void SellerTable::Load(const std::filesystem::path& path)
{
    for (auto& record : SellerScr::Load(path))
    {
        const std::int64_t shopId = record.shop_id;
        m_records.emplace(shopId, std::move(record));
    }
}

const SellerRecord* SellerTable::Find(std::int64_t shopId) const
{
    auto it = m_records.find(shopId);
    return it != m_records.end() ? &it->second : nullptr;
}
