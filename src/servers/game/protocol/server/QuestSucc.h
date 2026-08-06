#pragma once

#include <cstdint>
#include <vector>

class PayloadWriter;

struct QuestSuccItem
{
    std::uint32_t instance_id = 0;
    std::uint32_t item_id = 0;
    std::uint32_t qty_or_refine = 0;
    std::uint32_t option = 0;
    std::uint32_t option2 = 0;
    std::uint32_t time = 0;

    void Serialize(PayloadWriter& writer) const;
};

// GC_QUEST_SUCC (wire code 521064, s2c) -- server's fixed reply to
// CG_QUEST_RESULT, granting quest rewards.
struct QuestSucc
{
    std::vector<QuestSuccItem> items;
    std::uint32_t quest_id = 0;
    std::uint32_t money = 0;
    std::uint32_t fame = 0;
    std::uint32_t exp = 0;
    std::uint32_t ap = 0;
    std::uint32_t hp = 0;

    void Serialize(PayloadWriter& writer) const;
};
