#pragma once

#include "common/Dispatcher.h"

#include <cstdint>
#include <span>
#include <winsock2.h>

class TCPServer;
class LoginPacket;
class IDatabase;
class LoginSessionStore;

struct LoginContext
{
    TCPServer& server;
    SOCKET clientSocket;
    std::span<const uint8_t> key;
    IDatabase& db;
    LoginSessionStore& sessions;
};

class LoginDispatcher : public Dispatcher<LoginContext, LoginPacket>
{
public:
    LoginDispatcher();
};
