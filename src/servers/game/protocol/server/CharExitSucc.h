#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_CHAR_EXIT_SUCC (wire code 0x0007f71a / 522010, s2c) -- sent in
// response to CG_EXIT. The 4-byte body is never read by the client; real
// traffic shows it varying with no discernible pattern, consistent with
// unread noise rather than a real field, so it's sent as 0.
struct CharExitSucc : ServerMessage<GameOpcode::GC_CHAR_EXIT_SUCC>
{
    std::uint32_t unused = 0;

    void Serialize(PayloadWriter& writer) const override;
};
