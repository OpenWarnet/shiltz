#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// GC_STORE_OPEN_FAIL (wire code 531114, s2c) -- rejects CG_STORE_OPEN.
// reason defaults to 1 -- the only value seen so far (no bank_accounts row
// for this character, or the submitted password didn't match) -- but
// handlers/Store.cpp's sendFail() can override it per call site.
struct StoreOpenFail : ServerProtocol
{
    std::int32_t reason = 1;

    void Serialize(PayloadWriter& writer) const override;
};
