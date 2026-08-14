#include "StoreMoneySucc.h"

#include "common/PayloadWriter.h"

void StoreMoneySucc::Serialize(PayloadWriter& writer) const
{
    writer.Write(player_money);
    writer.Write(bank_negel);
    writer.Write(bank_remaining_cegel);
}
