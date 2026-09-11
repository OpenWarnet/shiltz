#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_EXIT (wire code 0x6457f / 411007, c2s). Fixed 4-byte body: the client
// echoes the opcode itself (7f 45 06 00 on the wire), same as CG_PLAY_START.
struct GameExit : ClientProtocol
{
    std::uint32_t opcode_echo = 0;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
