#pragma once

#include "protocol/Protocol.h"

#include <cstdint>

class PayloadWriter;

// Shared payload shape for GC_STORE_MONEY_IN_SUCC (521122) and
// GC_STORE_MONEY_OUT_SUCC (521123) -- same fields on the wire for both,
// only the opcode differs (see handlers/Store.cpp).
struct StoreMoneySucc : ServerProtocol
{
    // Character's wallet money after the transfer.
    std::int64_t player_money = 0;
    // Bank's money, split into "negel" units of 100,000,000 cegel each
    // (bank_negel = bank_money / 100000000) plus the sub-negel remainder
    // (bank_remaining_cegel = bank_money % 100000000) -- how the client
    // displays a bank balance too large for one field.
    std::int32_t bank_negel = 0;
    std::int32_t bank_remaining_cegel = 0;

    void Serialize(PayloadWriter& writer) const override;
};
