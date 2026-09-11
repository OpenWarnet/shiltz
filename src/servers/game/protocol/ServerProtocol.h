#pragma once

#include "GameOpcodes.h"
#include "protocol/Protocol.h"

class GamePacket;
class PayloadWriter;

struct ServerProtocol : GameProtocol
{
    static constexpr ProtocolDirection kDirection = ProtocolDirection::ServerToClient;

    ~ServerProtocol() override = default;

    virtual void Serialize(PayloadWriter& writer) const = 0;
    [[nodiscard]] virtual GameOpcode::Code Opcode() const = 0;

    [[nodiscard]] GamePacket Packet() const;
};

template <GameOpcode::Code OpcodeValue> struct ServerMessage : ServerProtocol
{
    static constexpr GameOpcode::Code kOpcode = OpcodeValue;

    [[nodiscard]] GameOpcode::Code Opcode() const final { return kOpcode; }
};
