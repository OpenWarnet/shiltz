#pragma once

#include "common/Dispatcher.h"

#include <cstdint>
#include <span>
#include <winsock2.h>

class TCPServer;
class GamePacket;

struct GameContext
{
    TCPServer& server;
    SOCKET clientSocket;
    std::span<const uint8_t> key;
};

class GameDispatcher : public Dispatcher<GameContext, GamePacket>
{
public:
    GameDispatcher();
};
