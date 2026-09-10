#include "GameServer.h"

#include "GamePacket.h"
#include "handlers/Inventory.h"

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
      m_simulation(*this, key, m_sessions, m_data),
      m_worldStrand(boost::asio::any_io_executor(IoContext().get_executor())), m_tickTimer(IoContext()),
      m_dbPool(1)
{
    m_data.Load();
    m_simulation.Start();

    // The other half of a pickup. The simulation decides who got the item,
    // on the world strand; this turns that into the database write and the
    // acknowledgement, which CompleteItemPickup puts on the DB pool.
    m_simulation.OnPickup([this](SOCKET socket, std::uint32_t itemNetworkId, std::uint32_t slotId,
                                 const Item& item) {
        CompleteItemPickup(
            GameContext{*this, socket, m_key, m_db, m_sessions, m_data, m_simulation, m_dbPool},
            itemNetworkId, slotId, item);
    });

    m_lastTick = std::chrono::steady_clock::now();
    ScheduleTick();
}

GameServer::~GameServer()
{
    m_tickTimer.cancel();
    m_simulation.Shutdown();
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

        // Same strand, so this never overlaps itself.
        m_simulation.Tick(std::chrono::duration<float>(delta).count());

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
        GameContext{*this, clientSocket, m_key, m_db, m_sessions, m_data, m_simulation, m_dbPool}, packet);
}

void GameServer::OnClientDisconnected(SOCKET clientSocket)
{
    // Leave first: it reads the session to find which maps this socket is
    // on, and the line below erases it.
    m_simulation.Leave(clientSocket);
    m_sessions.Remove(clientSocket);
}
