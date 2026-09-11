#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_STORE_PW_MODIFY_SUCC (wire code 521115, s2c) -- acknowledges a
// successful CG_STORE_PW_MODIFY. The int32 field's meaning isn't
// identified yet -- always sent as 0 (see handlers/Store.cpp).
struct StorePwModifySucc : ServerMessage<GameOpcode::GC_STORE_PW_MODIFY_SUCC>
{
    std::int32_t unknown = 0;

    void Serialize(PayloadWriter& writer) const override;
};
