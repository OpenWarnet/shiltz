#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

// One row of an itemNN.scr file -- Seal Online's per-item template table
// (name, stats, pricing, description, ...). Unlike monster.scr, these rows
// mix text (item name) and floating-point columns alongside integers, so
// this doesn't keep a generic ParseInt64'd `fields` vector the way
// MonsterRecord does -- only the columns actually needed are named here.
// Field layout reverse-engineered from the client's item loader: column 0
// is id, column 35 is buy_price, column 36 is sell_price. Add more named
// fields as more columns are needed.
struct ItemRecord
{
    std::int64_t id = 0;
    std::int64_t buy_price = 0;
    std::int64_t sell_price = 0;
};

class ItemScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<ItemRecord> Load(const std::filesystem::path& path);
};
