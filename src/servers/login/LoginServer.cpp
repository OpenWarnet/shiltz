#include "LoginServer.h"

#include "LoginPacket.h"

#include <iostream>

LoginServer::LoginServer(uint16_t port, std::span<const uint8_t> key,
                          std::span<const uint8_t> noncePayload, IDatabase& db)
    : Server(port, "Login"), m_key(key), m_noncePayload(noncePayload), m_db(db), m_dbPool(1)
{
}

void LoginServer::OnClientConnected(SOCKET clientSocket)
{
    // A peer that closes before we send (e.g. a bare TCP port-probe rather than
    // a real client) surfaces as WSAECONNRESET/WSAECONNABORTED in SendTo's log
    // here -- unrelated to the DES/nonce handshake despite the "Client
    // disconnected" that follows.
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

    m_dispatcher.Dispatch(LoginContext{*this, clientSocket, m_key, m_db, m_sessions, m_dbPool}, packet);
}
