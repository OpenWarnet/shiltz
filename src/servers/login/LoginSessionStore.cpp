#include "LoginSessionStore.h"

void LoginSessionStore::SetAccountId(SOCKET clientSocket, std::int64_t accountId)
{
    std::lock_guard lock(m_mutex);
    m_accountsBySocket[clientSocket] = accountId;
}

std::optional<std::int64_t> LoginSessionStore::GetAccountId(SOCKET clientSocket) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_accountsBySocket.find(clientSocket);
    if (it == m_accountsBySocket.end())
        return std::nullopt;

    return it->second;
}

void LoginSessionStore::SetSessionId(SOCKET clientSocket, std::int64_t sessionId)
{
    std::lock_guard lock(m_mutex);
    m_sessionIdsBySocket[clientSocket] = sessionId;
}

std::optional<std::int64_t> LoginSessionStore::GetSessionId(SOCKET clientSocket) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_sessionIdsBySocket.find(clientSocket);
    if (it == m_sessionIdsBySocket.end())
        return std::nullopt;

    return it->second;
}
