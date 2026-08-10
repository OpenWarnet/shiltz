#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>

// Handlers should depend on IDatabase / IStatement only, never a concrete
// backend, so OpenDatabase() can swap backends without touching call sites.
using SqlValue = std::variant<std::monostate, int64_t, double, std::string>;

class IStatement
{
public:
    virtual ~IStatement() = default;

    virtual void Bind(int index, SqlValue value) = 0; // index is 0-based.
    virtual bool Step() = 0;                           // false once exhausted.
    virtual SqlValue Column(int index) const = 0;       // valid for the current row only.
    virtual void Reset() = 0;                           // clears bindings too.
};

class IDatabase
{
public:
    virtual ~IDatabase() = default;

    virtual void Exec(const std::string& sql) = 0; // no result set (DDL, migrations).
    virtual std::unique_ptr<IStatement> Prepare(const std::string& sql) = 0;

    // Left to each backend -- "take the write lock up front" isn't portable
    // (SQLite: BEGIN IMMEDIATE; Postgres/MySQL: MVCC, conflicts at commit).
    virtual void BeginTransaction() = 0;
    virtual void Commit() = 0;
    virtual void Rollback() = 0;

    virtual int64_t LastInsertRowId() = 0; // from the most recent INSERT on this connection.
};

// Picks a backend from the connection string's scheme, e.g. "sqlite:db/login.sqlite3".
std::unique_ptr<IDatabase> OpenDatabase(const std::string& connectionString);
