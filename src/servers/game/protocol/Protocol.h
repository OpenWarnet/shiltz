#pragma once

#include <cstdint>

enum class ProtocolDirection : std::uint8_t
{
    ClientToServer,
    ServerToClient,
};

struct GameProtocol
{
    virtual ~GameProtocol() = default;
};
