#include "ItemDrop.h"

#include "common/PayloadReader.h"
#include "handlers/Inventory.h"

bool ItemDrop::Deserialize(PayloadReader& reader)
{
    return reader.Read(slot_id) && reader.Read(quantity);
}

void ItemDrop::Handle(const GameContext& ctx) const
{
    HandleItemDrop(ctx, *this);
}
