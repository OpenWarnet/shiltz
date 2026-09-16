#pragma once

#include "Outbox.h"
#include "Persistence.h"
#include "common/Server.h"
#include "storage/IDatabase.h"
#include "tables/GameData.h"
#include "world/World.h"

#include <chrono>
#include <cstdint>
#include <span>

class GameServer : public Server
{
public:
    GameServer(uint16_t port, std::span<const uint8_t> key, IDatabase& db);
    ~GameServer() override;

protected:
    void OnClientConnected(ConnectionId connection) override;
    void OnFrame(ConnectionId connection, std::span<const uint8_t> frame) override;
    void OnClientDisconnected(ConnectionId connection) override;

private:
    GameContext MakeContext(ConnectionId connection);

    // Re-arms m_tickTimer and, on the world strand, advances m_world.Tick()
    // by the actual elapsed time -- replaces the old dedicated tick thread
    // with a steady_timer posted on the io_context, serialized against
    // itself (never overlaps) by m_worldStrand. Independent of any
    // particular connection's strand -- the simulation advances on its own
    // schedule regardless of client traffic.
    void ScheduleTick();

    std::span<const uint8_t> m_key;
    Outbox m_outbox;
    GameData m_data;
    World m_world;

    Server::Strand m_worldStrand;
    boost::asio::steady_timer m_tickTimer;
    std::chrono::steady_clock::time_point m_lastTick;

    // Last member, so it's destroyed first and drains its jobs while everything they touch still exists.
    Persistence m_persistence;
};
