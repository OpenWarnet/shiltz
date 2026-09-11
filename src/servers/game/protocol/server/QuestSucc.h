#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>
#include <vector>

class PayloadWriter;

struct QuestSuccItem
{
    std::uint32_t inventory_id = 0;
    std::uint32_t slot_id = 0;
    std::uint32_t item_id = 0;
    std::uint32_t qty_or_refine = 0;
    std::uint64_t option = 0;
    std::uint32_t unknown2 = 0;

    void Serialize(PayloadWriter& writer) const;
};

// GC_QUEST_SUCC (wire code 521064, s2c) -- server's fixed reply to
// CG_QUEST_RESULT, granting quest rewards.
struct QuestSucc : ServerMessage<GameOpcode::GC_QUEST_SUCC>
{
    std::vector<QuestSuccItem> items;
    std::uint32_t quest_id = 0;
    std::uint64_t money = 0;
    std::uint32_t fame = 0;
    std::uint64_t exp = 0;
    std::uint32_t ap = 0;
    std::uint32_t hp = 0;

    void Serialize(PayloadWriter& writer) const override;
};
