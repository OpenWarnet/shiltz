#pragma once

#include "protocol/ClientProtocol.h"

#include <cstdint>

class PayloadReader;

// CG_STORE_CLOSE (wire code 411056, c2s) -- closes the bank storage UI.
// The single field is a constant 1 -- read for framing, not acted on (see
// handlers/Store.cpp).
struct StoreClose : ClientProtocol
{
    std::int32_t constant = 1;

    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
