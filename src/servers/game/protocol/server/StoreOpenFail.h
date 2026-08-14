#pragma once

#include <cstdint>

class PayloadWriter;

// GC_STORE_OPEN_FAIL (wire code 531114, s2c) -- rejects CG_STORE_OPEN.
// reason is always 1 (no bank_accounts row for this character, or the
// submitted password didn't match) -- there's no other cause on the wire
// (see handlers/Store.cpp).
struct StoreOpenFail
{
    std::int32_t reason = 1;

    void Serialize(PayloadWriter& writer) const;
};
