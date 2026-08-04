#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <winsock2.h>

// A resolved game session: which account owns this connection. Looked up
// from the `session` table (see LoginSessionStore -- the login server
// writes a row there on a successful CL_LOGIN, keyed by the session_id it
// later hands the client via GameConnectSuccess). Set by HandleEnter on
// CG_ENTER once it resolves that session_id; consulted by any later
// handler on the same connection that needs to know the owning account.
struct GameSession
{
    std::int64_t sessionId = 0;
    std::int64_t accountId = 0;
};

// Maps a connected socket to its resolved GameSession. Threaded through
// GameContext the same way IDatabase is -- GameServer owns one instance
// and hands out a reference per dispatched frame.
class GameSessionStore
{
public:
    void Set(SOCKET clientSocket, GameSession session);
    std::optional<GameSession> Get(SOCKET clientSocket) const;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<SOCKET, GameSession> m_sessionsBySocket;
};
