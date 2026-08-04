#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <winsock2.h>

// Maps a connected socket to the account that authenticated on it, and to
// the `session` table row created for that login. Both set by HandleLogin
// on success. account_id is consulted by any later handler on the same
// connection that needs to scope a DB write to "the account owning this
// connection" (e.g. character creation); session_id is handed to the game
// server (see HandleGameServerConnection) so it, in turn, can resolve
// account_id from the `session` table once a client connects to it and
// presents that id. Threaded through LoginContext the same way IDatabase
// is -- LoginServer owns one instance and hands out a reference per
// dispatched frame.
class LoginSessionStore
{
public:
    void SetAccountId(SOCKET clientSocket, std::int64_t accountId);
    std::optional<std::int64_t> GetAccountId(SOCKET clientSocket) const;

    void SetSessionId(SOCKET clientSocket, std::int64_t sessionId);
    std::optional<std::int64_t> GetSessionId(SOCKET clientSocket) const;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<SOCKET, std::int64_t> m_accountsBySocket;
    std::unordered_map<SOCKET, std::int64_t> m_sessionIdsBySocket;
};
