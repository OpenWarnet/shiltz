#pragma once

#include <cstdint>

class PayloadWriter;

// GC_LEVEL_UP_SUCC (wire code 521045, s2c) -- confirms CG_LEVEL_UP_CHECK,
// sent once the character's exp has crossed the threshold for `level`.
// unallocated_stat_points/unallocated_sp are the character's totals after
// adding level.scr's per-level grant for every level just gained;
// unallocated_ep is carried as-is -- ep hasn't been granted on level-up
// since a 2024 update (see LevelScr.h), so this is always the character's
// pre-existing ep total, never incremented here.
struct LevelUpSucc
{
    std::int32_t level = 0;
    std::int32_t unallocated_stat_points = 0;
    std::int32_t unallocated_sp = 0;
    std::int32_t unallocated_ep = 0;
    std::int64_t current_exp = 0;

    void Serialize(PayloadWriter& writer) const;
};
