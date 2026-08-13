#pragma once

#include "enums/JobId.h"
#include "parser/StatusScr.h"

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <unordered_map>

// Owns the status.scr per-class derived-combat-stat rate table.
class StatusTable
{
public:
    // status.scr blockIds. Blocks 7, 9, and 10 have no confirmed reader;
    // block 11 (kClownDamageBonusBlock) is genuinely sparse -- present
    // only for JobId::Clown -- rather than "loaded but unused".
    static constexpr std::size_t kDamageBlock = 0;
    static constexpr std::size_t kMagicBlock = 1;
    static constexpr std::size_t kDefenseBlock = 2;
    static constexpr std::size_t kAccuracyBlock = 3;
    static constexpr std::size_t kCriticalBlock = 4;
    static constexpr std::size_t kEvasionBlock = 5;
    static constexpr std::size_t kMaxHpBlock = 6;
    static constexpr std::size_t kApBlock = 8;
    static constexpr std::size_t kClownDamageBonusBlock = 11;

    // Throws std::runtime_error if the file can't be opened.
    void Load(const std::filesystem::path& path);

    // Looks up a status.scr rate by (block, jobId). Returns nullptr if
    // this table hasn't loaded that block/job combination -- always
    // true for kClownDamageBonusBlock except at JobId::Clown, since
    // that row simply doesn't exist for anyone else.
    const double* Find(std::size_t block, JobId jobId) const;

private:
    // Keyed by (classId << 32) | blockId -- see MakeKey in StatusTable.cpp.
    std::unordered_map<std::int64_t, double> m_rates;
};
