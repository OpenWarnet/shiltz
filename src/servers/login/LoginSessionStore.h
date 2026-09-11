#pragma once

#include "common/ConnectionId.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

// Maps a connection to the account/session set by HandleLogin on
// success. account_id lets later handlers on the same connection scope DB
// writes to the authenticated account (e.g. character creation); session_id
// is handed to the game server (see HandleGameServerConnection) so it can
// resolve account_id from the `session` table when the client presents that
// id there. Threaded through LoginContext like IDatabase -- LoginServer owns
// one instance and hands out a reference per dispatched frame.
class LoginSessionStore
{
public:
    void SetAccountId(ConnectionId connection, std::int64_t accountId);
    std::optional<std::int64_t> GetAccountId(ConnectionId connection) const;

    void SetSessionId(ConnectionId connection, std::int64_t sessionId);
    std::optional<std::int64_t> GetSessionId(ConnectionId connection) const;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<ConnectionId, std::int64_t> m_accountsByConnection;
    std::unordered_map<ConnectionId, std::int64_t> m_sessionIdsByConnection;
};
