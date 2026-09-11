#pragma once

#include "protocol/ClientProtocol.h"

class PayloadReader;

// Not on the wire: GameServer queues one when a connection is accepted, ahead of anything the client sends.
struct GameConnect : ClientProtocol
{
    bool Deserialize(PayloadReader& reader) override;
    void Handle(const GameContext& ctx) const override;
};
