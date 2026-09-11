#include "LoginSessionStore.h"

void LoginSessionStore::SetAccountId(ConnectionId connection, std::int64_t accountId)
{
    std::lock_guard lock(m_mutex);
    m_accountsByConnection[connection] = accountId;
}

std::optional<std::int64_t> LoginSessionStore::GetAccountId(ConnectionId connection) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_accountsByConnection.find(connection);
    if (it == m_accountsByConnection.end())
        return std::nullopt;

    return it->second;
}

void LoginSessionStore::SetSessionId(ConnectionId connection, std::int64_t sessionId)
{
    std::lock_guard lock(m_mutex);
    m_sessionIdsByConnection[connection] = sessionId;
}

std::optional<std::int64_t> LoginSessionStore::GetSessionId(ConnectionId connection) const
{
    std::lock_guard lock(m_mutex);
    auto it = m_sessionIdsByConnection.find(connection);
    if (it == m_sessionIdsByConnection.end())
        return std::nullopt;

    return it->second;
}
