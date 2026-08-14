#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

// One row of quest.scr -- Seal Online's NPC dialog-tree table, format
// version 5: 14 condition ints and 19 consequence ints per row (see
// QuestConditions/QuestConsequences below for the exact layout). Each row
// is one dialog node (group_id, talk_id); nodes sharing a group_id form one
// NPC's tree, linked by parent_idx. Only the columns the game server
// actually acts on are kept here -- group_id/talk_id/parent_idx and the
// player/npc dialog text are the client's concern (rendering the
// conversation), not the server's, so they're read past but not stored.
//
// action_id is what the client reports back on CG_QUEST_RESULT -- see
// tables/QuestTable.h for the action_id -> QuestActionRecord lookup this
// feeds, and handlers/Quest.cpp for how conditions/consequences are used.
struct QuestConditions
{
    std::int64_t has_item_0 = 0; // item id, paired with min_item_0_count
    std::int64_t min_item_0_count = 0;
    std::int64_t has_item_1 = 0;
    std::int64_t min_item_1_count = 0;
    std::int64_t has_flag = 0; // quest_flags flag id -- 0 means no flag required
    std::int64_t has_job = 0;  // job id -- 0 means no job required
    std::int64_t min_reputation = 0; // compared against Player::fame
    // Two unidentified condition ints (no known effect) sit between
    // min_reputation and min_level, and between min_level and min_days --
    // read past to keep every later column at its correct offset, but not
    // stored (see parser/QuestScr.cpp).
    std::int64_t min_level = 0;
    std::int64_t min_days = 0;    // no "days played" tracking yet -- always treated as satisfied
    std::int64_t min_cegel = 0;   // compared against Player::money ("Cegel" is Seal Online's currency)
    std::int64_t time_of_day = 0; // no server clock/day-night cycle yet -- always treated as satisfied
};

struct QuestConsequences
{
    std::int64_t reward_item_0 = 0;
    std::int64_t reward_item_0_count = 0;
    std::int64_t reward_item_1 = 0;
    std::int64_t reward_item_1_count = 0;
    std::int64_t reward_item_2 = 0;
    std::int64_t reward_item_2_count = 0;
    std::int64_t set_flag = 0; // quest_flags flag id to set -- 0 means nothing to set
    std::int64_t reward_cegel = 0;
    std::int64_t reward_exp = 0;
    std::int64_t reward_fame = 0;

    // Need a different server-side technique/packet than plain GC_QUEST_SUCC
    // (see handlers/Quest.cpp) -- not applied yet, just parsed and logged.
    std::int64_t teleport_map_id = 0;
    std::int64_t change_job_id = 0;
    std::int64_t add_skill_ids = 0;
    std::int64_t revival_point_id = 0;
    // One more unidentified consequence int (no known effect) follows --
    // read past, not stored.
};

struct QuestActionRecord
{
    std::int64_t action_id = 0;
    QuestConditions conditions;
    QuestConsequences consequences;
};

class QuestScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<QuestActionRecord> Load(const std::filesystem::path& path);
};
