#include "StoreClose.h"

#include "common/PayloadReader.h"
#include "handlers/Store.h"

bool StoreClose::Deserialize(PayloadReader& reader)
{
    return reader.Read(constant);
}

void StoreClose::Handle(const GameContext& ctx) const
{
    HandleStoreClose(ctx, *this);
}
