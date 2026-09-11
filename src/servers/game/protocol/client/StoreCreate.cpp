#include "StoreCreate.h"

#include "common/PayloadReader.h"
#include "handlers/Store.h"

bool StoreCreate::Deserialize(PayloadReader& reader)
{
    return reader.ReadString(password, 16);
}

void StoreCreate::Handle(const GameContext& ctx, Player&) const
{
    HandleStoreCreate(ctx, *this);
}
