#include "StoreMoneyIn.h"

#include "common/PayloadReader.h"
#include "handlers/Store.h"

bool StoreMoneyIn::Deserialize(PayloadReader& reader)
{
    return reader.Read(amount);
}

void StoreMoneyIn::Handle(const GameContext& ctx, Player&) const
{
    HandleStoreMoneyIn(ctx, *this);
}
