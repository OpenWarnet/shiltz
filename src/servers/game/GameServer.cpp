#include "GameServer.h"

#include "GamePacket.h"
#include "common/PayloadWriter.h"
#include "protocol/ClientProtocol.h"
#include "protocol/client/GameConnect.h"
#include "protocol/client/GameExit.h"
#include "world/common/Request.h"

#include <chrono>
#include <iostream>
#include <utility>

using namespace std::chrono_literals;

namespace
{
    constexpr auto kTickInterval = 100ms;
} // namespace

GameServer::GameServer(uint16_t port, std::span<const uint8_t> key, IDatabase& db)
    : Server(port, "Game"), m_key(key),
      m_outbox([this](ConnectionId to, std::span<const std::uint8_t> frame)
               { SendTo(to, frame); }),
      m_worldStrand(boost::asio::any_io_executor(IoContext().get_executor())), m_tickTimer(IoContext()),
      m_persistence(db, [this](std::function<void()> work)
                    { boost::asio::post(m_worldStrand, std::move(work)); })
{
    m_data.Load();
    m_world.Start(m_outbox, m_data);

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

void GameServer::OnClientConnected(ConnectionId connection)
{
    // Queued before the connection's first frame, so the world knows it before anything it sends.
    m_world.Receive(Request{.context = MakeContext(connection), .message = std::make_unique<GameConnect>()});
}

void GameServer::OnFrame(ConnectionId connection, std::span<const uint8_t> frame)
{
    GamePacket packet;
    if (!packet.Deserialize(frame, m_key))
        return;

    std::cout << "-> " << GameOpcode::Describe(packet.GetCode()) << " : " << frame.size()
              << " bytes, payload: " << packet.GetPayload().size() << " bytes\n";

    auto message = ClientProtocol::Create(packet);
    if (!message)
        return;

    m_world.Receive(Request{.context = MakeContext(connection), .message = std::move(message)});
}

void GameServer::OnClientDisconnected(ConnectionId connection)
{
    // Leave as a CG_EXIT through the same queue, so it runs after everything this client already sent.
    auto exit = std::make_unique<GameExit>();
    exit->disconnected = true;
    m_world.Receive(Request{.context = MakeContext(connection), .message = std::move(exit)});
}

GameContext GameServer::MakeContext(ConnectionId connection)
{
    return GameContext{connection, m_world, m_data, m_outbox, m_persistence};
}
