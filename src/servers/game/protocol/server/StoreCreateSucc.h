#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_STORE_CREATE_SUCC (wire code 521111, s2c) -- acknowledges a successful
// CG_STORE_CREATE. The single field is a constant 0.
struct StoreCreateSucc : ServerMessage<GameOpcode::GC_STORE_CREATE_SUCC>
{
    std::int32_t constant = 0;

    void Serialize(PayloadWriter& writer) const override;
};
