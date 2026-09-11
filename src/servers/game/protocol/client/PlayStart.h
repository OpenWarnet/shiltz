#pragma once

#include "protocol/ClientProtocol.h"

class PayloadReader;

// CG_PLAY_START has an as-yet unidentified payload. It is retained as an
// opaque client message so it can follow the same queued request path.
struct PlayStart : ClientProtocol
{
    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
