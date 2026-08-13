#pragma once

#include "parser/SetOptScr.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

// Owns the set_opt.scr equipped-set bonus table.
class SetOptionTable
{
public:
    // Throws std::runtime_error if the file can't be opened.
    void Load(const std::filesystem::path& path);

    // Looks up a set_opt.scr row by (ItemRecord::set_id, exact worn piece
    // count). Returns nullptr if this table hasn't loaded a row for that
    // exact pair -- most (set_id, piece_count) combinations don't have one
    // (e.g. a set might only grant a bonus at its full piece count, with no
    // partial-set row at all).
    const SetOptionRecord* Find(std::int64_t setId, std::int64_t pieceCount) const;

private:
    // Keyed by (setId << 32) | pieceCount -- see MakeKey in SetOptionTable.cpp.
    std::unordered_map<std::int64_t, SetOptionRecord> m_records;
};
