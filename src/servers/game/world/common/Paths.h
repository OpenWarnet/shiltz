#pragma once

#include <filesystem>

namespace Paths
{
struct DataDirectories
{
    std::filesystem::path root;
    std::filesystem::path monster_spawn;
    std::filesystem::path npc_spawn;
    std::filesystem::path items;
    std::filesystem::path skills;
};

inline const DataDirectories Data = []
{
    auto root =
        std::filesystem::path(SHILTZ_SOURCE_DIR) / "src" / "servers" / "game" / "world" / "data";

    return DataDirectories{
        .root = root,
        .monster_spawn = root / "spawn" / "monster",
        .npc_spawn = root / "spawn" / "npc",
        .items = root / "item",
        .skills = root / "skill",
    };
}();
} // namespace Paths