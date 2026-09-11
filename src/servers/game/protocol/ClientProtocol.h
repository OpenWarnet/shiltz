#pragma once

#include "protocol/Protocol.h"

#include <memory>

class GamePacket;
class PayloadReader;
struct GameContext;

struct ClientProtocol : GameProtocol
{
    static constexpr ProtocolDirection kDirection = ProtocolDirection::ClientToServer;

    [[nodiscard]] static std::unique_ptr<ClientProtocol> Create(const GamePacket& packet);

    ~ClientProtocol() override = default;
    virtual bool Deserialize(PayloadReader& reader) = 0;
    virtual void Handle(const GameContext& ctx) const = 0;
};
