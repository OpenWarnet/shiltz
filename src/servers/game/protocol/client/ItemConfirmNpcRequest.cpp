#include "ItemConfirmNpcRequest.h"

#include "common/PayloadReader.h"
#include "handlers/ItemConfirmNpc.h"

bool ItemConfirmNpcRequest::Deserialize(PayloadReader& reader)
{
    std::uint32_t count = 0;
    if (!reader.Read(count) || !reader.Read(npc_flag))
        return false;

    slot_ids.clear();
    slot_ids.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::uint32_t slotId = 0;
        if (!reader.Read(slotId))
            return false;

        slot_ids.push_back(slotId);
    }

    std::uint32_t padding = 0;
    return reader.Read(padding);
}

void ItemConfirmNpcRequest::Handle(const GameContext& ctx) const
{
    HandleItemConfirmNpcRequest(ctx, *this);
}
