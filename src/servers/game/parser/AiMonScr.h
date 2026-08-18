#pragma once

#include "ScrTable.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

// One row of ai_mon.scr -- Seal Online's per-monster AI script table.
// `id` (column 0) mirrors the row's own position in the file (0-based,
// i.e. row N in this vector has `id == N`); monster.scr's iAI_index and
// iAI_Index_On_Death columns are indexes into this same row-position
// space, joining a monster template to the AI row that drives it.
//
// Column layout, reverse-engineered from the loader (CRT_set_ai_script)
// and the two runtime AI-tick consumers (0x8139e88, 0x813b220):
//   0  id            -- row position; PROVEN redundant (equals every
//                        sample row's own file position) and never read
//                        back by the loader.
//   1  unknown_1      -- NOT in the reverse-engineered reference layout;
//                        this project's copy of the file carries one
//                        extra column here beyond that reference, of
//                        unidentified meaning. Usually 0; a small
//                        minority of rows carry a value in the same
//                        range as a skill_type/skill_id.
//   2  field_1        -- PROVEN dead weight: read by the loader but never
//                        stored anywhere (its jump-table entry is a
//                        straight fallthrough), and 0 on every row in
//                        this file.
//   3  pass_chance    -- PROVEN: compared against rand() % 100 at the top
//                        of both runtime AI ticks; a roll below this
//                        value means "use no skill this tick," before
//                        any skill_N pair is even considered.
//   4-15   six (skill_type, weight) picks, walked cumulatively after
//          pass_chance with a single 0..99 roll -- pass_chance plus the
//          six weights sums to 100 on most (not quite all) authored
//          rows. skill_type is PROVEN to be a row index into
//          crt_skill.scr (bounds-checked at load time against that
//          table's row count, and named for what the loader's own error
//          format string literally calls this cell: "skill_type").
//   16-33  three 6-field event-trigger groups (trigger_type, parameter,
//          cooldown_ms, max_uses, fire_chance, skill_type). Only each
//          group's skill_type (offset +5) is proven the same way the
//          picks' skill_type is -- bounds-checked at load time, named
//          from the loader's own "Event iSkill_ID error" message. The
//          other five fields in a group are copied into the record but
//          never read back by the loader's own validation, so their
//          names (matching a third-party reimplementation's `Trigger`
//          struct) describe the shape a conditioned skill use would
//          need, not an independently proven meaning.
struct AiMonRecord
{
    struct SkillPick
    {
        std::int64_t skill_type = 0;
        std::int64_t weight = 0;
    };

    struct Trigger
    {
        std::int64_t trigger_type = 0;
        std::int64_t parameter = 0;
        std::int64_t cooldown_ms = 0;
        std::int64_t max_uses = 0;
        std::int64_t fire_chance = 0;
        std::int64_t skill_type = 0;
    };

    static constexpr std::size_t kSkillPickCount = 6;
    static constexpr std::size_t kTriggerCount = 3;

    std::int64_t id = 0;
    std::int64_t unknown_1 = 0;
    std::int64_t field_1 = 0;
    std::int64_t pass_chance = 0;
    std::array<SkillPick, kSkillPickCount> skill_picks{};
    std::array<Trigger, kTriggerCount> triggers{};
};

class AiMonScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<AiMonRecord> Load(const std::filesystem::path& path);
};
