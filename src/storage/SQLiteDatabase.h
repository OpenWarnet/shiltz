#pragma once

#include "IDatabase.h"

struct sqlite3;

class SQLiteDatabase : public IDatabase
{
public:
    explicit SQLiteDatabase(const std::string& path);
    ~SQLiteDatabase() override;

    SQLiteDatabase(const SQLiteDatabase&) = delete;
    SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;

    void Exec(const std::string& sql) override;
    std::unique_ptr<IStatement> Prepare(const std::string& sql) override;
    int64_t LastInsertRowId() override;

    void BeginTransaction() override;
    void Commit() override;
    void Rollback() override;

private:
    sqlite3* m_db = nullptr;
};
