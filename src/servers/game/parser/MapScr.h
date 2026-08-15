#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// One row of map.scr -- Seal Online's per-map registry, joining a
// server_map_id to the basenames of the two files that populate that map's
// creature grid: monster_file (e.g. "m01", joins world/data/spawn/monster/)
// and npc_file (e.g. "npc01", joins world/data/spawn/npc/). Neither carries
// a directory or ".scr" extension -- callers append both. Most rows are
// unused map slots (server_map_id == -1, every other field blank/zero);
// callers should skip those.
struct MapRecord
{
    std::int64_t server_map_id = 0;
    std::string monster_file;
    std::string npc_file;
};

class MapScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<MapRecord> Load(const std::filesystem::path& path);
};
