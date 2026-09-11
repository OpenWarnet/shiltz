#include "ItemDelete.h"

#include "common/PayloadReader.h"
#include "handlers/Inventory.h"

bool ItemDelete::Deserialize(PayloadReader& reader)
{
    std::uint32_t confirmFlag = 0;
    std::uint32_t padding = 0;
    return reader.Read(slot_id) && reader.Read(confirmFlag) && reader.Read(padding);
}

void ItemDelete::Handle(const GameContext& ctx) const
{
    HandleItemDelete(ctx, *this);
}
