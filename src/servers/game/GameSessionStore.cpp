#include "GameSessionStore.h"

void GameSessionStore::Set(SOCKET clientSocket, GameSession session)
{
    std::lock_guard lock(m_mutex);
    m_sessionsBySocket[clientSocket] = session;
}

std::optional<GameSession> GameSessionStore::Get(SOCKET clientSocket) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_sessionsBySocket.find(clientSocket);
    if (it == m_sessionsBySocket.end())
        return std::nullopt;

    return it->second;
}
