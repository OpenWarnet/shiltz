#pragma once

#include "storage/IDatabase.h"

// RAII wrapper around a SQL transaction. Rolls back on destruction unless
// Commit() was called.
class DatabaseTransaction
{
public:
    explicit DatabaseTransaction(IDatabase& db);
    ~DatabaseTransaction();

    DatabaseTransaction(const DatabaseTransaction&) = delete;
    DatabaseTransaction& operator=(const DatabaseTransaction&) = delete;

    void Commit();

private:
    IDatabase& m_db;
    bool m_committed = false;
};
