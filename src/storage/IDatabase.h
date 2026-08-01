#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>

// Backend-agnostic database access. Handlers should depend on IDatabase /
// IStatement only, never on a concrete backend (sqlite3.h, a MySQL/Postgres
// client library, etc.) -- that's what lets OpenDatabase() swap the backend
// behind a connection string without touching call sites.
using SqlValue = std::variant<std::monostate, int64_t, double, std::string>;

class IStatement
{
public:
    virtual ~IStatement() = default;

    // index is 0-based.
    virtual void Bind(int index, SqlValue value) = 0;

    // Advances to the next row. Returns false once the statement is exhausted.
    virtual bool Step() = 0;

    // Valid only for the row made current by the last Step() that returned true.
    virtual SqlValue Column(int index) const = 0;

    // Rewinds so the statement can be Step()'d again (bindings are cleared).
    virtual void Reset() = 0;
};

class IDatabase
{
public:
    virtual ~IDatabase() = default;

    // Runs SQL with no result set (DDL, or one-off scripts during migration).
    virtual void Exec(const std::string& sql) = 0;

    virtual std::unique_ptr<IStatement> Prepare(const std::string& sql) = 0;
};

// Picks a backend from the connection string's scheme, e.g. "sqlite:db/login.sqlite3".
// Adding a "mysql://..." or "postgres://..." backend later means implementing
// IDatabase/IStatement for it and adding a branch here -- callers don't change.
std::unique_ptr<IDatabase> OpenDatabase(const std::string& connectionString);
