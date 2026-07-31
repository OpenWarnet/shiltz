#include "GameServer.h"

#include "GamePacket.h"

#include <iostream>

GameServer::GameServer(uint16_t port, std::span<const uint8_t> key)
    : TCPServer(port, "Game"), m_key(key)
{
}

void GameServer::OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame)
{
    GamePacket packet;
    if (!packet.Deserialize(frame, m_key))
        return;

    std::cout << "Received (" << frame.size() << " bytes, payload " << packet.GetPayload().size()
              << " bytes)\n";

    m_dispatcher.Dispatch(GameContext{*this, clientSocket, m_key}, packet);
}
