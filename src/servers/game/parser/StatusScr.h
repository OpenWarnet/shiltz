#pragma once

#include "ScrTable.h"

#include <cstdint>
#include <filesystem>
#include <vector>

// One status.scr row -- a single (class, block, value) triple. The real
// file is a sparse table, not a dense grid: most (class, block)
// combinations that could exist don't have a row at all -- e.g. block 11
// (the Clown-only damage bonus) has exactly one row in the whole file.
struct StatusRecord
{
    std::int64_t class_id = 0;
    std::int64_t block_id = 0;
    double value = 0;
};

// status.scr -- Seal Online's per-class derived-combat-stat rate table,
// sourced from the real client's own decoded data file (not a private
// server's reconstruction -- see World::kDamageBlock etc.'s comments for
// how each blockId was cross-checked against live client output).
// Covers 11 real classes (0-9, plus a "31" alias -- confirmed an exact
// duplicate of class 2/Knight, almost certainly a promoted tier that
// collapses to Knight's growth stats) across up to 11 blocks (0-10) each,
// plus one extra sparse row for block 11 (Clown-only, class 3 alone).
class StatusScr
{
public:
    // Throws std::runtime_error if the file can't be opened.
    static std::vector<StatusRecord> Load(const std::filesystem::path& path);
};
