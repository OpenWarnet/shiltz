#pragma once

#include <cstdint>

class PayloadWriter;

// GC_ENTER_FAIL (wire code 532050, s2c) -- rejects CG_ENTER. Static 4-byte
// body: a single reason code, no padding. `1` is the only reason currently
// in use (unknown session / account-username mismatch / no such character).
struct EnterFail
{
    std::uint32_t reason = 1;

    void Serialize(PayloadWriter& writer) const;
};
