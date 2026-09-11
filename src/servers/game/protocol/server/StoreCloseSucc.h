#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_STORE_CLOSE_SUCC (wire code 521117, s2c) -- acknowledges
// CG_STORE_CLOSE. The single field is a constant 0.
struct StoreCloseSucc : ServerMessage<GameOpcode::GC_STORE_CLOSE_SUCC>
{
    std::int32_t constant = 0;

    void Serialize(PayloadWriter& writer) const override;
};
