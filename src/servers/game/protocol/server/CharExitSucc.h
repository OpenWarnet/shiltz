#pragma once

#include <cstdint>

class PayloadWriter;

// GC_CHAR_EXIT_SUCC (wire code 0x0007f71a / 522010, s2c) -- sent in
// response to CG_EXIT. Body is CONFIRMED fixed at 4 bytes (a single u32) by
// OpenShiltz's server-side trace (game/handlers/gc_char_exit_succ.py), but
// the client's own parser was independently confirmed to never read the
// body pointer at all -- a genuinely unused payload, like several other
// "notification" packets in this project. Real captures show the 4 bytes
// varying (0x112, 0x167, 0x111, 0x3d, no obvious pattern) which is
// consistent with it being unread noise rather than a real field, so this
// is just sent as 0.
struct CharExitSucc
{
    std::uint32_t unused = 0;

    void Serialize(PayloadWriter& writer) const;
};
