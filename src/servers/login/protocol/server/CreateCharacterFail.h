#pragma once

#include <cstdint>

class PayloadWriter;

// LC_CREATECHAR_FAIL (wire code 231006, s2c) -- rejects CL_CREATE_CHARACTER.
// Static 4-byte body: a single reason code, no padding. `6` is the only
// reason currently in use (client-side validation failure -- empty/too
// short/too long name, or a missing job).
struct CreateCharacterFail
{
    std::uint32_t reason = 6;

    void Serialize(PayloadWriter& writer) const;
};
