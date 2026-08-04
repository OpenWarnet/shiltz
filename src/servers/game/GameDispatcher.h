#pragma once

#include "common/Dispatcher.h"

#include <cstdint>
#include <span>
#include <winsock2.h>

class TCPServer;
class GamePacket;
class IDatabase;
class GameSessionStore;

struct GameContext
{
    TCPServer& server;
    SOCKET clientSocket;
    std::span<const uint8_t> key;
    IDatabase& db;
    GameSessionStore& sessions;
};

class GameDispatcher : public Dispatcher<GameContext, GamePacket>
{
public:
    GameDispatcher();
};
