#pragma once

#include <boost/asio/thread_pool.hpp>

#include <cstdint>
#include <span>
#include <winsock2.h>

class Server;
class Outbox;
class Persistence;
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

    // Legacy raw posts to the DB thread; new code uses persistence instead.
    boost::asio::thread_pool& dbPool;

    const Outbox& outbox;
    Persistence& persistence;
};
