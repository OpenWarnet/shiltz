#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_LEVEL_UP_FAIL (wire code 531066, s2c) -- rejects CG_LEVEL_UP_CHECK,
// echoing back the character's unchanged level/exp.
struct LevelUpFail : ServerProtocol
{
    std::int32_t level = 0;
    std::int32_t exp = 0;

    void Serialize(PayloadWriter& writer) const override;
};
