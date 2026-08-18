#pragma once

#include "GameDispatcher.h"
#include "GameSessionStore.h"
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
    void OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame) override;
    void OnClientDisconnected(SOCKET clientSocket) override;

private:
    // Re-arms m_tickTimer and, on the world strand, advances m_world.Tick()
    // by the actual elapsed time -- replaces the old dedicated tick thread
    // with a steady_timer posted on the io_context, serialized against
    // itself (never overlaps) by m_worldStrand. Independent of any
    // particular connection's strand -- the simulation advances on its own
    // schedule regardless of client traffic.
    void ScheduleTick();

    // Turns each map's World::Tick() result into GC_CRT_MOVE/
    // GC_ATTACK_CRT2TARGET_MISS broadcasts -- one per creature move/
    // attack, sent only to sessions on that map whose own 3x3 zone view
    // (Map::ZonesAround, same rule as HandleMovement in
    // handlers/Movement.cpp) currently covers the creature's zone. Called
    // from ScheduleTick, still on the world strand.
    void BroadcastCreatureUpdates(const std::vector<MapTickResult>& tickResults);

    GameDispatcher m_dispatcher;
    std::span<const uint8_t> m_key;
    IDatabase& m_db;
    GameSessionStore m_sessions;
    GameData m_data;
    World m_world;

    Server::Strand m_worldStrand;
    boost::asio::steady_timer m_tickTimer;
    std::chrono::steady_clock::time_point m_lastTick;

    // Dedicated worker for blocking IDatabase (SQLite) calls a handler
    // wants off the reactor pool's threads -- see GameContext::dbPool. One
    // thread is enough: both servers only ever hold a single sqlite3*
    // connection each, so a bigger pool wouldn't add real concurrency.
    boost::asio::thread_pool m_dbPool;
};
