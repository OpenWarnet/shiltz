#pragma once

#include "GameDispatcher.h"
#include "GameSessionStore.h"
#include "common/TCPServer.h"
#include "storage/IDatabase.h"
#include "world/World.h"

#include <cstdint>
#include <span>

class GameServer : public TCPServer
{
public:
    GameServer(uint16_t port, std::span<const uint8_t> key, IDatabase& db);
    ~GameServer() override;

protected:
    void OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame) override;
    void OnClientDisconnected(SOCKET clientSocket) override;

private:
    GameDispatcher m_dispatcher;
    std::span<const uint8_t> m_key;
    IDatabase& m_db;
    GameSessionStore m_sessions;
    World m_world;
};
