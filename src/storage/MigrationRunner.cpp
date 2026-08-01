#include "MigrationRunner.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

void RunMigrations(IDatabase& db, const std::filesystem::path& migrationsDir)
{
    db.Exec("CREATE TABLE IF NOT EXISTS schema_migrations ("
            "name TEXT PRIMARY KEY, "
            "applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(migrationsDir))
    {
        if (entry.path().extension() == ".sql")
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files)
    {
        const std::string name = file.filename().string();

        auto alreadyApplied = db.Prepare("SELECT 1 FROM schema_migrations WHERE name = ?");
        alreadyApplied->Bind(0, name);
        if (alreadyApplied->Step())
        {
            continue;
        }

        std::ifstream stream(file);
        std::stringstream script;
        script << stream.rdbuf();

        std::cout << "Applying migration: " << name << "\n";
        db.Exec(script.str());

        auto markApplied = db.Prepare("INSERT INTO schema_migrations (name) VALUES (?)");
        markApplied->Bind(0, name);
        markApplied->Step();
    }
}
