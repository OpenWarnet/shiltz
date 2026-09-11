#include "ServerProtocol.h"

#include "GamePacket.h"
#include "common/PayloadWriter.h"

GamePacket ServerProtocol::Packet() const
{
    PayloadWriter writer;
    Serialize(writer);
    return GamePacket(Opcode(), writer.Data());
}
