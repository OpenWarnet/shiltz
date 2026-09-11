#pragma once

#include "protocol/Protocol.h"

#include <memory>

class GamePacket;
class PayloadReader;
struct GameContext;
struct Player;

// A message that doesn't need its sender in the world (CG_ENTER, CG_EXIT, ...).
struct ClientProtocol : GameProtocol
{
    static constexpr ProtocolDirection kDirection = ProtocolDirection::ClientToServer;

    [[nodiscard]] static std::unique_ptr<ClientProtocol> Create(const GamePacket& packet);

    ~ClientProtocol() override = default;
    virtual bool Deserialize(PayloadReader& reader) = 0;
    virtual void Handle(const GameContext& ctx) const = 0;
};

// A message from a player in the world; dropped if the sender's connection has no Player.
struct PlayerMessage : ClientProtocol
{
    void Handle(const GameContext& ctx) const final;
    virtual void Handle(const GameContext& ctx, Player& player) const = 0;
};
