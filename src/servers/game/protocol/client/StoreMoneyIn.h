#pragma once

#include <cstdint>

class PayloadReader;

// CG_STORE_MONEY_IN (wire code 411060, c2s) -- deposit money from the
// character's wallet into the bank.
struct StoreMoneyIn
{
    std::int64_t amount = 0;

    bool Deserialize(PayloadReader& reader);
};
