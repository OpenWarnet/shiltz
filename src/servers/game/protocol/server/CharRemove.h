#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_CHAR_REMOVE (wire code 0x7cc21 / 511009, s2c) -- another character left this client's view or the map.
struct CharRemove : ServerMessage<GameOpcode::GC_CHAR_REMOVE>
{
    std::uint32_t id = 0;

    void Serialize(PayloadWriter& writer) const override;
};
