#include "GameServer.h"

#include "GamePacket.h"

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

namespace
{
    constexpr auto kTickInterval = 100ms;
    // Log a heartbeat every 5s (50 ticks at 100ms) rather than every tick,
    // just to make the loop's liveness visible on stdout without spamming it.
    constexpr int kTicksPerHeartbeat = 50;
} // namespace

GameServer::GameServer(uint16_t port, std::span<const uint8_t> key, IDatabase& db)
    : TCPServer(port, "Game"), m_key(key), m_db(db)
{
    m_data.Load();
    m_world.Start();

    m_ticking = true;
    m_tickThread = std::thread([this] { RunTickLoop(); });
}

GameServer::~GameServer()
{
    m_ticking = false;
    if (m_tickThread.joinable())
        m_tickThread.join();

    m_world.Shutdown();
}

void GameServer::RunTickLoop()
{
    auto last = std::chrono::steady_clock::now();
    int ticksSinceHeartbeat = 0;

    while (m_ticking)
    {
        std::this_thread::sleep_for(kTickInterval);

        const auto now = std::chrono::steady_clock::now();
        const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last);
        last = now;

        m_world.Tick(delta);

        if (++ticksSinceHeartbeat >= kTicksPerHeartbeat)
        {
            ticksSinceHeartbeat = 0;
            std::cout << "World tick alive\n";
        }
    }
}

void GameServer::OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame)
{
    GamePacket packet;
    if (!packet.Deserialize(frame, m_key))
        return;

    std::cout << "Received (" << frame.size() << " bytes, payload " << packet.GetPayload().size()
              << " bytes)\n";

    m_dispatcher.Dispatch(GameContext{*this, clientSocket, m_key, m_db, m_sessions, m_world, m_data},
                          packet);
}

void GameServer::OnClientDisconnected(SOCKET clientSocket)
{
    m_sessions.Remove(clientSocket);
}
