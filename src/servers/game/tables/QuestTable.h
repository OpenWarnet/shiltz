#pragma once

#include "parser/QuestScr.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

// Owns the quest.scr dialog-tree table, indexed by action_id -- the id the
// client reports back via CG_QUEST_RESULT (see protocol/client/
// QuestResult.h and handlers/Quest.cpp). group_id/talk_id/parent_idx and
// the dialog text itself aren't kept (see parser/QuestScr.h) since the
// server only ever needs to resolve one action_id at a time to its
// conditions/consequences.
//
// action_id 0 and -1 both mean "this dialog node has no action attached"
// (the vast majority of quest.scr's ~16.5k rows) and are never inserted.
// Nonzero action_ids are effectively unique across the file except for a
// single known collision (action_id 1, used by two unrelated rows in the
// shipped data) -- the later row in file order wins for that one id, and
// Load() logs when this happens so a future data update that introduces a
// new collision doesn't silently shadow a real reward.
class QuestTable
{
public:
    // Throws std::runtime_error if the file can't be opened.
    void Load(const std::filesystem::path& path);

    const QuestActionRecord* Find(std::int64_t actionId) const;

private:
    std::unordered_map<std::int64_t, QuestActionRecord> m_records;
};
