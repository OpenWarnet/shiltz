#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_CHAR_STATUS_UP_FAIL (wire code 531049, s2c) -- rejects
// CG_CHAR_STATUS_UP, reporting the character's unchanged unallocated stat
// points (the request wasn't affordable, or named an unknown stat_id).
struct CharStatusUpFail : ServerProtocol
{
    std::int32_t unallocated_point_remaining = 0;

    void Serialize(PayloadWriter& writer) const override;
};
