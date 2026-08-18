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
    // Log a heartbeat every 5s (50 ticks at 100ms) rather than every tick,
    // just to make the loop's liveness visible on stdout without spamming it.
    constexpr int kTicksPerHeartbeat = 50;

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

        BroadcastCreatureMoves(m_world.Tick(delta));

        if (++m_ticksSinceHeartbeat >= kTicksPerHeartbeat)
        {
            m_ticksSinceHeartbeat = 0;
            std::cout << "World tick alive\n";
        }

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

void GameServer::BroadcastCreatureMoves(const std::vector<MapTickResult>& tickResults)
{
    for (const auto& mapResult : tickResults)
    {
        if (mapResult.creature_moves.empty())
            continue;

        // The map produced these moves a moment ago on the map-pool thread,
        // so it's still loaded.
        Map* map = m_world.GetMap(mapResult.server_map_id);
        if (!map)
            continue;

        // Everyone currently on this map -- "can see the monster" is then a
        // per-player zone check below, same as HandleMovement's own
        // known_zones logic (handlers/Movement.cpp).
        const auto players = map->Players();
        if (players.empty())
            continue;

        for (const auto& move : mapResult.creature_moves)
        {
            CrtMove crtMove{
                .creature_id = move.creature_id,
                .x = static_cast<std::uint32_t>(move.from_x),
                .y = static_cast<std::uint32_t>(move.from_y),
                .target_x = static_cast<std::uint32_t>(move.to_x),
                .target_y = static_cast<std::uint32_t>(move.to_y),
                .speed_raw = 0,
            };

            PayloadWriter writer;
            crtMove.Serialize(writer);
            GamePacket packet(GameOpcode::GC_CRT_MOVE, writer.Data());
            const auto payload = packet.Serialize(m_key);

            const auto creatureZone = Map::ZoneOf(move.to_x, move.to_y);

            for (const auto& player : players)
            {
                if (Contains(map->ZonesAround(player.x, player.y), creatureZone))
                    SendTo(player.socket, payload);
            }
        }
    }
}
