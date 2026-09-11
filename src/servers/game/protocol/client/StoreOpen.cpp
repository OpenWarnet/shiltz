#include "StoreOpen.h"

#include "common/PayloadReader.h"
#include "handlers/Store.h"

bool StoreOpen::Deserialize(PayloadReader& reader)
{
    return reader.ReadString(password, 16);
}

void StoreOpen::Handle(const GameContext& ctx, Player& player) const
{
    HandleStoreOpen(ctx, *this, player);
}
