#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

// One row of warp.scr -- Seal Online's warp-destination table. Unlike every
// other .scr table, a row has no id column of its own: `warp_id` is purely
// positional, derived from the row's order in the file (the first data row,
// right after the header/count line, is warp_id 0, and so on). This is the
// id-space QuestConsequences::warp_id (quest.scr's old teleport_map_id
// column, renamed to match) joins against to know where a quest teleport
// should send the player.
struct WarpRecord
{
    std::int64_t warp_id = 0;
    std::int64_t server_map_id = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
};

class WarpScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<WarpRecord> Load(const std::filesystem::path& path);
};
