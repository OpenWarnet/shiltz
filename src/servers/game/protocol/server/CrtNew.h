#pragma once

#include "protocol/ServerProtocol.h"
#include "protocol/server/CrtLoad.h"

class PayloadWriter;

// GC_CRT_NEW (wire code 0x07CC36 / 511030, s2c) -- introduces one creature
// using the same 112-byte record as GC_CRT_LOAD, without a leading count.
struct CrtNew : ServerMessage<GameOpcode::GC_CRT_NEW>
{
    CrtLoadRecord record;

    void Serialize(PayloadWriter& writer) const override;
};
