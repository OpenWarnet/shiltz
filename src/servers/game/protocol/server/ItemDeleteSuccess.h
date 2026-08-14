#pragma once

#include <cstdint>

class PayloadWriter;

// GC_ITEM_DELETE_SUCC (wire code 0x07F4F6, s2c) -- acknowledges a
// successful CG_ITEM_DELETE. 32-byte body: slot_id (echoes the request's
// wire slot_id) followed by 7 reserved dwords. Real captures show a
// nonzero dword at index 5 (e.g. 0x0B5D0A) that stays constant across
// multiple deletes within the same session but differs across sessions --
// plausibly gold or some other per-character counter unrelated to the
// deleted item, but not confirmed, so it's zeroed here rather than
// guessed at.
struct ItemDeleteSuccess
{
    std::uint32_t slot_id = 0;

    void Serialize(PayloadWriter& writer) const;
};
