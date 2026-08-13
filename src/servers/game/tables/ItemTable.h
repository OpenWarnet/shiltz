#pragma once

#include "parser/ItemScr.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

// Owns the itemNN.scr per-item template table.
class ItemTable
{
public:
    // itemNN.scr is split across many files (item.scr, item1.scr, ...,
    // item31.scr) rather than one table -- loads every ".scr" file in the
    // given directory instead of naming each one, so a new file dropped in
    // later is picked up without a code change. Throws std::runtime_error
    // if a file can't be opened.
    void Load(const std::filesystem::path& dir);

    // Looks up an itemNN.scr row by ItemRecord::id -- the same id-space as
    // inventory_slot.item_id. Returns nullptr if this table hasn't loaded
    // that item id.
    const ItemRecord* Find(std::int64_t itemId) const;

private:
    std::unordered_map<std::int64_t, ItemRecord> m_records;
};
