#pragma once

#include "LoginDispatcher.h"
#include "LoginSessionStore.h"
#include "common/Server.h"
#include "storage/IDatabase.h"

#include <cstdint>
#include <span>

class LoginServer : public Server
{
public:
    LoginServer(uint16_t port, std::span<const uint8_t> key, std::span<const uint8_t> noncePayload,
                IDatabase& db);

protected:
    void OnClientConnected(ConnectionId connection) override;
    void OnFrame(ConnectionId connection, std::span<const uint8_t> frame) override;

private:
    LoginDispatcher m_dispatcher;
    std::span<const uint8_t> m_key;
    std::span<const uint8_t> m_noncePayload;
    IDatabase& m_db;
    LoginSessionStore m_sessions;

    // Dedicated worker for blocking IDatabase (SQLite) calls a handler
    // wants off the reactor pool's threads -- see LoginContext::dbPool.
    // One thread is enough: both servers only ever hold a single sqlite3*
    // connection each, so a bigger pool wouldn't add real concurrency.
    boost::asio::thread_pool m_dbPool;
};
