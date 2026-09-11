#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// Shared payload shape for GC_STORE_MONEY_IN_SUCC (521122) and
// GC_STORE_MONEY_OUT_SUCC (521123) -- same fields on the wire for both,
// only the opcode differs (see handlers/Store.cpp). Use the
// StoreMoneyInSucc / StoreMoneyOutSucc names below.
template <GameOpcode::Code OpcodeValue> struct StoreMoneySucc : ServerMessage<OpcodeValue>
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

// Defined (and instantiated) in StoreMoneySucc.cpp.
extern template struct StoreMoneySucc<GameOpcode::GC_STORE_MONEY_IN_SUCC>;
extern template struct StoreMoneySucc<GameOpcode::GC_STORE_MONEY_OUT_SUCC>;

using StoreMoneyInSucc = StoreMoneySucc<GameOpcode::GC_STORE_MONEY_IN_SUCC>;
using StoreMoneyOutSucc = StoreMoneySucc<GameOpcode::GC_STORE_MONEY_OUT_SUCC>;
