#pragma once

#include "GameDispatcher.h"
#include "GameSessionStore.h"
#include "common/TCPServer.h"
#include "storage/IDatabase.h"
#include "tables/GameData.h"
#include "world/World.h"

#include <atomic>
#include <cstdint>
#include <span>
#include <thread>

class GameServer : public TCPServer
{
public:
    GameServer(uint16_t port, std::span<const uint8_t> key, IDatabase& db);
    ~GameServer() override;

protected:
    void OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame) override;
    void OnClientDisconnected(SOCKET clientSocket) override;

private:
    // Runs on m_tickThread: sleeps in fixed 100ms slices and calls
    // m_world.Tick() with the actual elapsed time each time it wakes, until
    // m_ticking is cleared (see the destructor). Independent of the
    // per-connection threads TCPServer spawns -- the simulation advances on
    // its own schedule regardless of client traffic.
    void RunTickLoop();

    GameDispatcher m_dispatcher;
    std::span<const uint8_t> m_key;
    IDatabase& m_db;
    GameSessionStore m_sessions;
    GameData m_data;
    World m_world;

    std::atomic<bool> m_ticking{false};
    std::thread m_tickThread;
};
