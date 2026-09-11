#include "ItemMove.h"

#include "common/PayloadReader.h"
#include "handlers/Inventory.h"

bool ItemMove::Deserialize(PayloadReader& reader)
{
    return reader.Read(source_slot_id) && reader.Read(dest_slot_id);
}

void ItemMove::Handle(const GameContext& ctx) const
{
    HandleItemMove(ctx, *this);
}
