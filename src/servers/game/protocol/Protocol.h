#pragma once

#include <cstdint>

class PayloadWriter;

enum class ProtocolDirection : std::uint8_t
{
    ClientToServer,
    ServerToClient,
};

struct GameProtocol
{
    virtual ~GameProtocol() = default;
};

struct ServerProtocol : GameProtocol
{
    static constexpr ProtocolDirection kDirection = ProtocolDirection::ServerToClient;

    ~ServerProtocol() override = default;
    virtual void Serialize(PayloadWriter& writer) const = 0;
};
