#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <winsock2.h>

// Maps a connected socket to the account/session set by HandleLogin on
// success. account_id lets later handlers on the same connection scope DB
// writes to the authenticated account (e.g. character creation); session_id
// is handed to the game server (see HandleGameServerConnection) so it can
// resolve account_id from the `session` table when the client presents that
// id there. Threaded through LoginContext like IDatabase -- LoginServer owns
// one instance and hands out a reference per dispatched frame.
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
