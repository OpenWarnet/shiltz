#include "ItemTradeSell.h"

#include "common/PayloadReader.h"
#include "handlers/Trade.h"

bool ItemTradeSell::Deserialize(PayloadReader& reader)
{
    return reader.Read(slot_id) && reader.Read(count) && reader.Read(instance_id);
}

void ItemTradeSell::Handle(const GameContext& ctx) const
{
    HandleItemTradeSell(ctx, *this);
}
