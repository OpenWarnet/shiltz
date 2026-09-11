#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_STORE_PW_MODIFY_FAIL (wire code 531116, s2c) -- rejects
// CG_STORE_PW_MODIFY. reason is a guess at the field's purpose (not
// confirmed against a real client) -- this handler only ever sends 1,
// for either "no bank_accounts row" or "old password mismatch" (see
// handlers/Store.cpp, which doesn't distinguish the two on the wire).
struct StorePwModifyFail : ServerMessage<GameOpcode::GC_STORE_PW_MODIFY_FAIL>
{
    std::int32_t reason = 1;

    void Serialize(PayloadWriter& writer) const override;
};
