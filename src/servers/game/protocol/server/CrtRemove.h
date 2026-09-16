#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_CRT_REMOVE (wire code 0x07CC37 / 511031, s2c) -- removes one creature
// from a client's view. Captured lethal attacks leave the dead creature
// loaded for its death animation; later REMOVE packets target other ids.
struct CrtRemove : ServerMessage<GameOpcode::GC_CRT_REMOVE>
{
    std::uint32_t creature_id = 0;

    void Serialize(PayloadWriter& writer) const override;
};
