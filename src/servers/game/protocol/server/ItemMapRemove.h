#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_ITEM_MAP_REMOVE (wire code 511036, s2c) -- tells the client to remove
// a ground item entity from its view of the map. Static 4-byte body: the
// ground item's instance id (see Item::id / ItemMapNew.id).
struct ItemMapRemove : ServerMessage<GameOpcode::GC_ITEM_MAP_REMOVE>
{
    std::uint32_t id = 0;

    void Serialize(PayloadWriter& writer) const override;
};
