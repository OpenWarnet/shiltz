#pragma once

#include "protocol/ServerProtocol.h"

#include <cstdint>

class PayloadWriter;

// GC_ITEM_CONFIRM_NPC_FAIL (wire code 0x081B23, s2c) -- rejects
// CG_ITEM_CONFIRM_NPC_REQUEST. See enums/ItemConfirmFailReason.h for
// `result_code`'s values. NoSlotsAppraised (3) is the only one this
// handler ever actually sends -- there's no per-cause code on the wire,
// so a bad slot index, an ineligible item type, an unmet level
// requirement, and insufficient money are all indistinguishable to the
// client (see handlers/ItemConfirmNpc.h).
struct ItemConfirmNpcFail : ServerMessage<GameOpcode::GC_ITEM_CONFIRM_NPC_FAIL>
{
    std::int32_t result_code = 3;

    void Serialize(PayloadWriter& writer) const override;
};
