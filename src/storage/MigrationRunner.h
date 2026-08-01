#pragma once

#include "IDatabase.h"

#include <filesystem>

// Applies every *.sql file in migrationsDir, in filename order, that isn't
// already recorded in the schema_migrations table. Filenames should sort in
// the order they must run, e.g. "0001_create_accounts.sql".
void RunMigrations(IDatabase& db, const std::filesystem::path& migrationsDir);
