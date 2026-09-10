#include "GameServer.h"

#include "GamePacket.h"
#include "common/PayloadWriter.h"
#include "protocol/server/CrtMove.h"

#include <algorithm>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

namespace
{
    constexpr auto kTickInterval = 100ms;

    bool Contains(const std::vector<std::pair<std::int32_t, std::int32_t>>& zones,
                  const std::pair<std::int32_t, std::int32_t>& zone)
    {
        return std::find(zones.begin(), zones.end(), zone) != zones.end();
    }
} // namespace

GameServer::GameServer(uint16_t port, std::span<const uint8_t> key, IDatabase& db)
    : Server(port, "Game"), m_key(key), m_db(db),
      m_worldStrand(boost::asio::any_io_executor(IoContext().get_executor())), m_tickTimer(IoContext()),
      m_dbPool(1)
{
    m_data.Load();
    m_world.Start();

    m_lastTick = std::chrono::steady_clock::now();
    ScheduleTick();
}

GameServer::~GameServer()
{
    m_tickTimer.cancel();
    m_world.Shutdown();
}

void GameServer::ScheduleTick()
{
    m_tickTimer.expires_after(kTickInterval);
    m_tickTimer.async_wait(boost::asio::bind_executor(m_worldStrand, [this](boost::system::error_code ec) {
        if (ec)
            return; // cancelled (shutdown) or timer destroyed

        const auto now = std::chrono::steady_clock::now();
        const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTick);
        m_lastTick = now;

        m_world.Tick(delta);

        ScheduleTick();
    }));
}

void GameServer::OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame)
{
    GamePacket packet;
    if (!packet.Deserialize(frame, m_key))
        return;

    std::cout << "Received (" << frame.size() << " bytes, payload " << packet.GetPayload().size()
              << " bytes)\n";

    m_dispatcher.Dispatch(
        GameContext{*this, clientSocket, m_key, m_db, m_sessions, m_world, m_data, m_dbPool}, packet);
}

void GameServer::OnClientDisconnected(SOCKET clientSocket)
{
    // Look up the session before removing it -- it's the only place that
    // still knows which map (if any) this socket's MapPlayer entry is on.
    if (auto session = m_sessions.Get(clientSocket))
    {
        if (Map* map = m_world.GetMap(session->player.map_id))
            map->RemovePlayer(clientSocket);
    }

    m_sessions.Remove(clientSocket);
}

