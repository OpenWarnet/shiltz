#pragma once

#include "protocol/ClientProtocol.h"

class PayloadReader;

// CG_EXIT has no payload.
struct GameExit : ClientProtocol
{
    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
