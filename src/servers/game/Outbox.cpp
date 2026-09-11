#include "Outbox.h"

#include "GamePacket.h"
#include "protocol/ServerProtocol.h"

#include <utility>

Outbox::Outbox(SendFn send) : m_send(std::move(send))
{
}

void Outbox::Send(ConnectionId to, const ServerProtocol& message) const
{
    m_send(to, Frame(message));
}

std::vector<std::uint8_t> Outbox::Frame(const ServerProtocol& message)
{
    // Server -> client isn't encrypted, so no key.
    return message.Packet().Serialize();
}
