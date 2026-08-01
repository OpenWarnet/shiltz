#include "IDatabase.h"
#include "SQLiteDatabase.h"

#include <stdexcept>
#include <string_view>

std::unique_ptr<IDatabase> OpenDatabase(const std::string& connectionString)
{
    constexpr std::string_view kSqlitePrefix = "sqlite:";
    if (connectionString.starts_with(kSqlitePrefix))
    {
        return std::make_unique<SQLiteDatabase>(connectionString.substr(kSqlitePrefix.size()));
    }

    throw std::runtime_error("Unsupported database connection string: " + connectionString);
}
