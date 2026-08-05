#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

struct MonsterSpawnInstance
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t direction = 0;
};

struct MonsterSpawnGroup
{
    std::int64_t monster_id = 0;
    std::vector<MonsterSpawnInstance> instances;
};

struct MonsterSpawnTable
{
    bool has_direction = false;
    std::vector<MonsterSpawnGroup> groups;
};

class MonsterSpawnScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static MonsterSpawnTable Load(const std::filesystem::path& path);
};
