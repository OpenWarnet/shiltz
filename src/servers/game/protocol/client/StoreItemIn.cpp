#include "StoreItemIn.h"

#include "common/PayloadReader.h"
#include "handlers/Store.h"

bool StoreItemIn::Deserialize(PayloadReader& reader)
{
    return reader.Read(inventory_slot_id) && reader.Read(bank_slot_id) && reader.Read(amount);
}

void StoreItemIn::Handle(const GameContext& ctx, Player&) const
{
    HandleStoreItemIn(ctx, *this);
}
