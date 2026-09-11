#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_CHAR_STATUS_UP (wire code 411018, c2s) -- client asks to allocate an
// unallocated stat point into one of the six raw stats. `stat_id` follows
// the client's own str/int/dex/con/men/sen ordering (see
// CharacterDataLoad's stats_str..stats_sen wire fields), 1-based: 1=str,
// 2=int, 3=dex, 4=con, 5=men, 6=sen.
struct CharStatusUp : PlayerMessage
{
    std::int32_t stat_id = 0;
    std::int32_t amount = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx, Player& player) const override;
};
