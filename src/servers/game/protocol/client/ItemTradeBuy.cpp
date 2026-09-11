#include "ItemTradeBuy.h"

#include "common/PayloadReader.h"
#include "handlers/Trade.h"

bool ItemTradeBuy::Deserialize(PayloadReader& reader)
{
    return reader.Read(shop_id) && reader.Read(item_buy_index) && reader.Read(amount) &&
           reader.Read(slot_id) && reader.Read(creature_instance_id);
}

void ItemTradeBuy::Handle(const GameContext& ctx, Player&) const
{
    HandleItemTradeBuy(ctx, *this);
}
