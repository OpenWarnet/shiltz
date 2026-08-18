#pragma once

#include "GameOpcodes.h"
#include "common/Dispatcher.h"

#include <boost/asio/thread_pool.hpp>

#include <cstdint>
#include <span>
#include <winsock2.h>

class Server;
class GamePacket;
class IDatabase;
class GameSessionStore;
class World;
class GameData;

struct GameContext
{
    Server& server;
    SOCKET clientSocket;
    std::span<const uint8_t> key;
    IDatabase& db;
    GameSessionStore& sessions;
    World& world;
    const GameData& data;

    // Blocking IDatabase calls (SQLite) don't belong on the reactor pool's
    // threads -- a handler that needs one should boost::asio::post(dbPool,
    // ...) the DB work and reply via server.SendTo() (safe from any
    // thread), rather than calling ctx.db directly inline. Only a couple of
    // handlers do this so far (see LevelUp.cpp); the rest still call ctx.db
    // synchronously, which is an accepted interim -- see CLAUDE.md/the
    // Asio migration plan.
    boost::asio::thread_pool& dbPool;
};

class GameDispatcher : public Dispatcher<GameContext, GamePacket, GameOpcode::Code>
{
public:
    GameDispatcher();
};
