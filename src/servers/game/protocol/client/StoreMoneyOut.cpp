#include "StoreMoneyOut.h"

#include "common/PayloadReader.h"
#include "handlers/Store.h"

bool StoreMoneyOut::Deserialize(PayloadReader& reader)
{
    return reader.Read(amount);
}

void StoreMoneyOut::Handle(const GameContext& ctx, Player& player) const
{
    HandleStoreMoneyOut(ctx, *this, player);
}
