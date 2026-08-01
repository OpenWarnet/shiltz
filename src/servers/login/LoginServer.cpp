#include "LoginServer.h"

#include "LoginPacket.h"

#include <iostream>

LoginServer::LoginServer(uint16_t port, std::span<const uint8_t> key,
                          std::span<const uint8_t> noncePayload, IDatabase& db)
    : TCPServer(port, "Login"), m_key(key), m_noncePayload(noncePayload), m_db(db)
{
}

void LoginServer::OnClientConnected(SOCKET clientSocket)
{
    // If the peer already closed the connection before we even called send()
    // (e.g. a bare TCP health-check/port-probe that connects then hangs up
    // immediately, rather than a client that speaks the login protocol),
    // this is where SendTo's failure log shows up -- WSAECONNRESET/
    // WSAECONNABORTED here means the "Client disconnected" that follows has
    // nothing to do with the DES/nonce handshake at all.
    if (SendTo(clientSocket, m_noncePayload))
        std::cout << "Sent step-0 nonce frame (" << m_noncePayload.size() << " bytes)\n";
}

void LoginServer::OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame)
{
    LoginPacket packet;
    if (!packet.Deserialize(frame, m_key))
        return;

    std::cout << "Received (" << frame.size() << " bytes, payload " << packet.GetPayload().size()
              << " bytes)\n";

    m_dispatcher.Dispatch(LoginContext{*this, clientSocket, m_key, m_db}, packet);
}
