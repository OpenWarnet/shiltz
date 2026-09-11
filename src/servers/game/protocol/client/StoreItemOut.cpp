#include "StoreItemOut.h"

#include "common/PayloadReader.h"
#include "handlers/Store.h"

bool StoreItemOut::Deserialize(PayloadReader& reader)
{
    return reader.Read(inventory_slot_id) && reader.Read(bank_slot_id) && reader.Read(amount) &&
           reader.Read(unknown);
}

void StoreItemOut::Handle(const GameContext& ctx, Player& player) const
{
    HandleStoreItemOut(ctx, *this, player);
}
