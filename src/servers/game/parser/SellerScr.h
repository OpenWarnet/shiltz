#pragma once

#include "ScrTable.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

// One row of seller.scr -- Seal Online's per-shop item listing table.
// `shop_id` is the same id-space as MonsterRecord::seller_id, joining a
// monster/NPC to the shop it opens. Each row is a fixed 30-slot item list
// (item_0..item_29); unused slots are 0. Field layout reverse-engineered
// from the client's shop loader: one pipe-delimited decimal token per
// column, column 0 is shop_id and columns 1..30 are the item slots, so
// `items[i]` corresponds directly to token index `i + 1`.
struct SellerRecord
{
    static constexpr std::size_t kItemCount = 30;

    std::int64_t shop_id = 0;
    std::array<std::int64_t, kItemCount> items{};
};

class SellerScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<SellerRecord> Load(const std::filesystem::path& path);
};
