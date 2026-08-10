#include "Transaction.h"

DatabaseTransaction::DatabaseTransaction(IDatabase& db) : m_db(db)
{
    m_db.BeginTransaction();
}

DatabaseTransaction::~DatabaseTransaction()
{
    if (!m_committed)
    {
        try
        {
            m_db.Rollback();
        }
        catch (...)
        {
            // Best-effort -- a destructor must not throw.
        }
    }
}

void DatabaseTransaction::Commit()
{
    m_db.Commit();
    m_committed = true;
}
