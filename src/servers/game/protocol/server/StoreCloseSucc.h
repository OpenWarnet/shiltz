#pragma once

#include <cstdint>

class PayloadWriter;

// GC_STORE_CLOSE_SUCC (wire code 521117, s2c) -- acknowledges
// CG_STORE_CLOSE. The single field is a constant 0.
struct StoreCloseSucc
{
    std::int32_t constant = 0;

    void Serialize(PayloadWriter& writer) const;
};
