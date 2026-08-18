#pragma once

#include "parser/AiMonScr.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

// Owns the ai_mon.scr monster-AI table.
class AiMonTable
{
public:
    // Throws std::runtime_error if the file can't be opened.
    void Load(const std::filesystem::path& path);

    // Looks up an ai_mon.scr row by AiMonRecord::id -- the same id-space as
    // MonsterRecord::iAI_index / iAI_Index_On_Death. Returns nullptr if
    // this table hasn't loaded that id.
    const AiMonRecord* Find(std::int64_t id) const;

private:
    std::unordered_map<std::int64_t, AiMonRecord> m_records;
};
