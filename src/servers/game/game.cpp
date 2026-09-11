#include "GameServer.h"
#include "common/PacketCapture.h"
#include "storage/IDatabase.h"
#include "storage/MigrationRunner.h"

#include <array>
#include <filesystem>

int main()
{
    std::array<uint8_t, 28> kBlowfishKey = {
        0x64, 0x6a, 0x78, 0x6f, 0x72, 0x45, 0x6b, 0x64, 0x64, 0x50, 0x74, 0x54, 0x6a, 0x66,
        0x21, 0x40, 0x29, 0x28, 0x21, 0x72, 0x6d, 0x61, 0x6b, 0x73, 0x67, 0x6f, 0x21, 0x00};

    // Same DB the login server writes accounts/characters/sessions to --
    // RunMigrations is idempotent, so it's safe to call regardless of
    // whether the login server has already started (or ever starts) first.
    const std::filesystem::path sourceDir(SHILTZ_SOURCE_DIR);
    const auto dbPath = sourceDir / "db" / "login.sqlite3";
    const auto migrationsDir = sourceDir / "db" / "migrations" / "sqlite";

    auto db = OpenDatabase("sqlite:" + dbPath.string());
    RunMigrations(*db, migrationsDir);

    PacketCapture::Init("game");

    GameServer server(8081, kBlowfishKey, *db);
    server.Run();

    return 0;
}
