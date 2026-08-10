#include "GameServer.h"

#include "GamePacket.h"

#include <iostream>

GameServer::GameServer(uint16_t port, std::span<const uint8_t> key, IDatabase& db)
    : TCPServer(port, "Game"), m_key(key), m_db(db)
{
    m_world.Start();
}

GameServer::~GameServer()
{
    m_world.Shutdown();
}

void GameServer::OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame)
{
    GamePacket packet;
    if (!packet.Deserialize(frame, m_key))
        return;

    std::cout << "Received (" << frame.size() << " bytes, payload " << packet.GetPayload().size()
              << " bytes)\n";

    m_dispatcher.Dispatch(GameContext{*this, clientSocket, m_key, m_db, m_sessions, m_world}, packet);
}

void GameServer::OnClientDisconnected(SOCKET clientSocket)
{
    m_sessions.Remove(clientSocket);
}
