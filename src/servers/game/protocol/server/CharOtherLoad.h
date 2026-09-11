#pragma once

#include "protocol/ServerProtocol.h"
#include "protocol/server/CharOtherRecord.h"

#include <cstddef>
#include <vector>

class PayloadWriter;

// GC_CHAR_OTHER_LOAD (wire code 0x7cc1b / 511003, s2c) -- characters that came into view when this
// client's own view changed: `count` (u32) followed by `count` records.
struct CharOtherLoad : ServerMessage<GameOpcode::GC_CHAR_OTHER_LOAD>
{
    // The real server never puts more in one packet; bigger batches go out as several.
    static constexpr std::size_t kMaxRecords = 11;

    std::vector<CharOtherRecord> records;

    void Serialize(PayloadWriter& writer) const override;
};
