#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>
#include <vector>

class PayloadWriter;

struct ItemConfirmNpcResult
{
    std::uint32_t slot_id = 0;
    std::uint64_t option_bits = 0;

    void Serialize(PayloadWriter& writer) const;
};

struct ItemConfirmNpcSucc : ServerMessage<GameOpcode::GC_ITEM_CONFIRM_NPC_SUCC>
{
    std::vector<ItemConfirmNpcResult> results;
    std::uint32_t total_fee = 0;

    void Serialize(PayloadWriter& writer) const override;
};
