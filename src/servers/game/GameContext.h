#pragma once

#include <boost/asio/thread_pool.hpp>

#include <cstdint>
#include <span>
#include <winsock2.h>

class Server;
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
    // thread), rather than calling ctx.db directly inline.
    boost::asio::thread_pool& dbPool;
};
