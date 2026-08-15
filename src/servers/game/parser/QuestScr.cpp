#include "QuestScr.h"

#include <iostream>
#include <string>
#include <string_view>

namespace
{
// v5 layout: group_id, talk_id (2) + 14 conditions + parent_idx + player +
// npc (3) + action_id (1) + 19 consequences = 39 columns per complete row.
// See parser/QuestScr.h for the v3 -> v5 diff (two extra condition ints,
// one extra trailing consequence int).
constexpr std::size_t kExpectedColumns = 39;

QuestActionRecord BuildRecord(const std::vector<std::string>& row)
{
    auto at = [&](std::size_t i) -> std::string_view { return row[i]; };

    QuestActionRecord record;
    record.action_id = ScrTable::ParseInt64(at(19));

    QuestConditions& c = record.conditions;
    c.has_item_0 = ScrTable::ParseInt64(at(2));
    c.min_item_0_count = ScrTable::ParseInt64(at(3));
    c.has_item_1 = ScrTable::ParseInt64(at(4));
    c.min_item_1_count = ScrTable::ParseInt64(at(5));
    c.has_flag = ScrTable::ParseInt64(at(6));
    c.has_job = ScrTable::ParseInt64(at(7));
    // index 8 (reserved_28) skipped.
    c.min_reputation = ScrTable::ParseInt64(at(9));
    // index 10 (unidentified condition int) skipped.
    c.min_level = ScrTable::ParseInt64(at(11));
    // index 12 (unidentified condition int) skipped.
    c.min_days = ScrTable::ParseInt64(at(13));
    c.min_cegel = ScrTable::ParseInt64(at(14));
    c.time_of_day = ScrTable::ParseInt64(at(15));

    QuestConsequences& q = record.consequences;
    q.reward_item_0 = ScrTable::ParseInt64(at(20));
    q.reward_item_0_count = ScrTable::ParseInt64(at(21));
    q.reward_item_1 = ScrTable::ParseInt64(at(22));
    q.reward_item_1_count = ScrTable::ParseInt64(at(23));
    q.reward_item_2 = ScrTable::ParseInt64(at(24));
    q.reward_item_2_count = ScrTable::ParseInt64(at(25));
    q.set_flag = ScrTable::ParseInt64(at(26));
    q.reward_cegel = ScrTable::ParseInt64(at(27));
    q.reward_exp = ScrTable::ParseInt64(at(28));
    q.reward_fame = ScrTable::ParseInt64(at(29));
    // indices 30-33 (reserved_12..15) skipped.
    q.warp_id = ScrTable::ParseInt64(at(34));
    q.change_job_id = ScrTable::ParseInt64(at(35));
    q.add_skill_ids = ScrTable::ParseInt64(at(36));
    q.revival_point_id = ScrTable::ParseInt64(at(37));
    // index 38 (unidentified consequence int) skipped.

    return record;
}

// Streaming state for OnLine below -- holds a partially-assembled row while
// waiting for its remainder. Needed because a handful of rows in the wild
// (e.g. quest.scr's "The seeds of fear" and "Are you referring to the
// person that's behind the screen..." player-choice lines) contain a
// literal newline inside their Pstr text, which ScrTable's line-based
// tokenizer sees as two separate lines: the first stops short of
// kExpectedColumns columns because the text field's closing '|' hasn't
// been reached yet, and the second starts mid-field with an empty leading
// token. Re-joining across the line break with "\n" reconstructs the
// original text and puts every later column back in its correct position.
// Any row that already has all of its columns (the overwhelming majority)
// round-trips through this untouched.
struct PendingRow
{
    std::vector<std::string> tokens;

    bool Empty() const { return tokens.empty(); }

    void Start(const std::vector<std::string_view>& row)
    {
        tokens.assign(row.begin(), row.end());
    }

    void Continue(const std::vector<std::string_view>& row)
    {
        if (row.empty())
            return;

        tokens.back() += '\n';
        tokens.back() += row.front();
        tokens.insert(tokens.end(), row.begin() + 1, row.end());
    }
};

void OnLine(std::vector<QuestActionRecord>& records, PendingRow& pending, std::size_t lineIndex,
            const std::vector<std::string_view>& tokens)
{
    if (lineIndex == 0)
    {
        std::int64_t count = ScrTable::ParseInt64(tokens[0]);
        if (count > 0)
        {
            records.reserve(static_cast<std::size_t>(count));
        }

        return;
    }

    if (pending.Empty())
    {
        pending.Start(tokens);
    }
    else
    {
        pending.Continue(tokens);
    }

    if (pending.tokens.size() < kExpectedColumns)
    {
        return; // still short a closing '|' somewhere -- wait for more lines
    }

    if (pending.tokens.size() != kExpectedColumns)
    {
        std::cout << "QuestScr: row ending at line " << lineIndex << " has "
                  << pending.tokens.size() << " columns, expected " << kExpectedColumns
                  << " -- skipping\n";
    }
    else
    {
        records.push_back(BuildRecord(pending.tokens));
    }

    pending.tokens.clear();
}
} // namespace

std::vector<QuestActionRecord> QuestScr::Load(const std::filesystem::path& path)
{
    std::vector<QuestActionRecord> records;
    PendingRow pending;

    ScrTable::Load(path, [&](std::size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, pending, lineIndex, tokens);
    });

    if (!pending.Empty())
    {
        std::cout << "QuestScr: file ended mid-row (" << pending.tokens.size()
                  << " of " << kExpectedColumns << " columns) -- discarding\n";
    }

    return records;
}
