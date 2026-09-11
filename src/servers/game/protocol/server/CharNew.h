#pragma once

#include "protocol/ServerProtocol.h"
#include "protocol/server/CharOtherRecord.h"

class PayloadWriter;

// GC_CHAR_NEW (wire code 0x7cc1a / 511002, s2c) -- another character came into this client's view.
struct CharNew : ServerMessage<GameOpcode::GC_CHAR_NEW>
{
    CharOtherRecord record;

    void Serialize(PayloadWriter& writer) const override;
};
