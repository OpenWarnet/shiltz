#pragma once

#include "protocol/Protocol.h"

#include <cstdint>
#include <vector>

class PayloadWriter;

// GC_VIEW_REMOVE_ALL (wire code 511041, s2c) -- tells the client to drop a
// batch of entities from its current view. Wire shape is three back-to-back
// id arrays (`count` u32 followed by `count` u32 ids each), in order:
// player_ids, creature_ids, item_ids. Only creature_ids is populated for
// now -- player/item view tracking isn't implemented yet, so those two
// arrays are always sent empty.
struct ViewRemoveAll : ServerProtocol
{
    std::vector<std::uint32_t> player_ids;
    std::vector<std::uint32_t> creature_ids;
    std::vector<std::uint32_t> item_ids;

    void Serialize(PayloadWriter& writer) const override;
};
