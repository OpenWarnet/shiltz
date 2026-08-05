#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

struct NpcInstance
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t direction = 0;
};

struct NpcSpawn
{
    std::int64_t unknown = 0;
    std::int64_t id = 0;
    std::vector<NpcInstance> instances;
};

class NpcScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<NpcSpawn> Load(const std::filesystem::path& path);
};
