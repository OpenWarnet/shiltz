#include "SQLiteDatabase.h"

#include <sqlite3.h>

#include <stdexcept>
#include <type_traits>

namespace
{
    class SQLiteStatement : public IStatement
    {
    public:
        explicit SQLiteStatement(sqlite3_stmt* stmt) : m_stmt(stmt) {}

        ~SQLiteStatement() override { sqlite3_finalize(m_stmt); }

        void Bind(int index, SqlValue value) override
        {
            const int column = index + 1; // sqlite3 binds are 1-based
            std::visit(
                [&](auto&& v)
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::monostate>)
                        sqlite3_bind_null(m_stmt, column);
                    else if constexpr (std::is_same_v<T, int64_t>)
                        sqlite3_bind_int64(m_stmt, column, v);
                    else if constexpr (std::is_same_v<T, double>)
                        sqlite3_bind_double(m_stmt, column, v);
                    else if constexpr (std::is_same_v<T, std::string>)
                        sqlite3_bind_text(m_stmt, column, v.c_str(), static_cast<int>(v.size()),
                                           SQLITE_TRANSIENT);
                },
                value);
        }

        bool Step() override
        {
            const int rc = sqlite3_step(m_stmt);
            if (rc == SQLITE_ROW)
                return true;
            if (rc == SQLITE_DONE)
                return false;

            throw std::runtime_error(std::string("SQLite step failed: ") +
                                      sqlite3_errmsg(sqlite3_db_handle(m_stmt)));
        }

        SqlValue Column(int index) const override
        {
            switch (sqlite3_column_type(m_stmt, index))
            {
                case SQLITE_INTEGER:
                    return static_cast<int64_t>(sqlite3_column_int64(m_stmt, index));
                case SQLITE_FLOAT:
                    return sqlite3_column_double(m_stmt, index);
                case SQLITE_TEXT:
                    return std::string(reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, index)),
                                        static_cast<size_t>(sqlite3_column_bytes(m_stmt, index)));
                default:
                    return std::monostate{};
            }
        }

        void Reset() override
        {
            sqlite3_reset(m_stmt);
            sqlite3_clear_bindings(m_stmt);
        }

    private:
        sqlite3_stmt* m_stmt;
    };
}

SQLiteDatabase::SQLiteDatabase(const std::string& path)
{
    if (sqlite3_open(path.c_str(), &m_db) != SQLITE_OK)
    {
        const std::string message = sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        throw std::runtime_error("Failed to open SQLite database '" + path + "': " + message);
    }

    // Without this, a concurrent writer (another thread, or the other
    // server process sharing this DB file) gets SQLITE_BUSY immediately
    // instead of waiting for the in-progress transaction to finish.
    sqlite3_busy_timeout(m_db, 5000);
}

SQLiteDatabase::~SQLiteDatabase()
{
    sqlite3_close(m_db);
}

void SQLiteDatabase::Exec(const std::string& sql)
{
    char* error = nullptr;
    if (sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK)
    {
        const std::string message = error ? error : "unknown error";
        sqlite3_free(error);
        throw std::runtime_error("SQLite exec failed: " + message);
    }
}

std::unique_ptr<IStatement> SQLiteDatabase::Prepare(const std::string& sql)
{
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("SQLite prepare failed: " + std::string(sqlite3_errmsg(m_db)));

    return std::make_unique<SQLiteStatement>(stmt);
}

int64_t SQLiteDatabase::LastInsertRowId()
{
    return sqlite3_last_insert_rowid(m_db);
}

void SQLiteDatabase::BeginTransaction()
{
    // IMMEDIATE takes the write lock up front instead of deferring it to
    // the first write, so a conflict is caught at the start, not at COMMIT.
    Exec("BEGIN IMMEDIATE");
}

void SQLiteDatabase::Commit()
{
    Exec("COMMIT");
}

void SQLiteDatabase::Rollback()
{
    Exec("ROLLBACK");
}
