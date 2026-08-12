#pragma once

#include <cstdint>

// GC_ITEM_CONFIRM_NPC_FAIL's result_code -- see
// protocol/server/ItemConfirmNpcFail.h for the full table.
enum class ItemConfirmFailReason : std::int32_t
{
    NoSlotsAppraised = 3,
    Reserved = 10, // not sent by this handler
};
