#pragma once

#include "common/Dispatcher.h"

#include <boost/asio/thread_pool.hpp>

#include <cstdint>
#include <span>
#include <winsock2.h>

class Server;
class LoginPacket;
class IDatabase;
class LoginSessionStore;

struct LoginContext
{
    Server& server;
    SOCKET clientSocket;
    std::span<const uint8_t> key;
    IDatabase& db;
    LoginSessionStore& sessions;

    // Blocking IDatabase calls (SQLite) don't belong on the reactor pool's
    // threads -- a handler that needs one should boost::asio::post(dbPool,
    // ...) the DB work and reply via server.SendTo() (safe from any
    // thread), rather than calling ctx.db directly inline. Only a couple of
    // handlers do this so far (see Auth.cpp); the rest still call ctx.db
    // synchronously, which is an accepted interim -- see CLAUDE.md/the
    // Asio migration plan.
    boost::asio::thread_pool& dbPool;
};

class LoginDispatcher : public Dispatcher<LoginContext, LoginPacket>
{
public:
    LoginDispatcher();
};
