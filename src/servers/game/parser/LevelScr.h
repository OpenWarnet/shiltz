#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

// One row of level.scr -- Seal Online's per-level exp requirement table.
// `exp` is the amount of exp a character at `level` must exceed to advance
// to `level + 1` (not a cumulative total) -- e.g. row "1|42|0|0|" means a
// level-1 character needs more than 42 exp to become level 2.
// `stat_points_gained`/`sp_gained` are the unallocated stat/skill points
// awarded for making that same level -> level+1 jump. There used to be a
// fifth column for enchant points (ep), but ep hasn't been granted on
// level-up since a 2024 update (see
// http://forum.playrohan.com/forum/showthread.php?t=44514), so it's not
// modeled here.
struct LevelRecord
{
    std::int64_t level = 0;
    std::int64_t exp = 0;
    std::int64_t stat_points_gained = 0;
    std::int64_t sp_gained = 0;
};

class LevelScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<LevelRecord> Load(const std::filesystem::path& path);
};
