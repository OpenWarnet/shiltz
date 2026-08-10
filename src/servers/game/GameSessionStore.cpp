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

bool GameSessionStore::TryClaimCharacter(std::int64_t characterId, SOCKET clientSocket)
{
    std::lock_guard lock(m_mutex);
    auto [it, inserted] = m_socketByCharacterId.try_emplace(characterId, clientSocket);
    return inserted || it->second == clientSocket;
}

void GameSessionStore::ReleaseCharacterClaim(std::int64_t characterId)
{
    std::lock_guard lock(m_mutex);
    m_socketByCharacterId.erase(characterId);
}

void GameSessionStore::Remove(SOCKET clientSocket)
{
    std::lock_guard lock(m_mutex);
    auto it = m_sessionsBySocket.find(clientSocket);
    if (it == m_sessionsBySocket.end())
        return;

    auto claimIt = m_socketByCharacterId.find(it->second.characterId);
    if (claimIt != m_socketByCharacterId.end() && claimIt->second == clientSocket)
        m_socketByCharacterId.erase(claimIt);

    m_sessionsBySocket.erase(it);
}
