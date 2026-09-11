#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_CHAR_STATUS_UP_SUCC (wire code 521048, s2c) -- confirms
// CG_CHAR_STATUS_UP, reporting the stat's new value and how many
// unallocated points are left.
struct CharStatusUpSucc : ServerProtocol
{
    std::int32_t stat_id = 0;
    std::int32_t current_stat_point = 0;
    std::int32_t unallocated_point_remaining = 0;

    void Serialize(PayloadWriter& writer) const override;
};
