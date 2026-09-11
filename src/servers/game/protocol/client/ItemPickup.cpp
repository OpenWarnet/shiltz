#include "ItemPickup.h"

#include "common/PayloadReader.h"
#include "handlers/Inventory.h"

bool ItemPickup::Deserialize(PayloadReader& reader)
{
    return reader.Read(id) && reader.Read(slot_id);
}

void ItemPickup::Handle(const GameContext& ctx, Player&) const
{
    HandleItemPickup(ctx, *this);
}
