#pragma once

#include "parser/SellerScr.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

// Owns the seller.scr shop-listing table.
class SellerTable
{
public:
    // Throws std::runtime_error if the file can't be opened.
    void Load(const std::filesystem::path& path);

    // Looks up a seller.scr row by SellerRecord::shop_id -- the same
    // id-space as MonsterRecord::seller_id. Returns nullptr if this table
    // hasn't loaded that shop id.
    const SellerRecord* Find(std::int64_t shopId) const;

private:
    std::unordered_map<std::int64_t, SellerRecord> m_records;
};
