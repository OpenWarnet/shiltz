#pragma once

#include "parser/WarpScr.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

// Owns the warp.scr warp-destination table.
class WarpTable
{
public:
    // Throws std::runtime_error if the file can't be opened.
    void Load(const std::filesystem::path& path);

    // Looks up a warp.scr row by WarpRecord::warp_id -- the same id-space as
    // QuestConsequences::warp_id. Returns nullptr if this table hasn't
    // loaded that warp id.
    const WarpRecord* Find(std::int64_t warpId) const;

private:
    std::unordered_map<std::int64_t, WarpRecord> m_records;
};
