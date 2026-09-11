#include "StoreMoneySucc.h"

#include "common/PayloadWriter.h"

template <GameOpcode::Code OpcodeValue>
void StoreMoneySucc<OpcodeValue>::Serialize(PayloadWriter& writer) const
{
    writer.Write(player_money);
    writer.Write(bank_negel);
    writer.Write(bank_remaining_cegel);
}

template struct StoreMoneySucc<GameOpcode::GC_STORE_MONEY_IN_SUCC>;
template struct StoreMoneySucc<GameOpcode::GC_STORE_MONEY_OUT_SUCC>;
