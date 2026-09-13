#pragma once

#include <optional>

struct Character;
struct CharacterDerivedStats;
class StatusTable;

// Computes the job/level/raw-stat baseline of CharacterDerivedStats: no
// equipment, no buffs. Uses StatusTable's status.scr rate tables plus the
// per-class constants in RawStatCalculator.cpp. See EquipmentStatCalculator for
// the other half of the pipeline -- stats/Stats.h's RecalculateDerivedStats
// combines both via operator+(CharacterDerivedStats).
class RawStatCalculator
{
public:
    // Returns nullopt if character.job_id can't be resolved to a status.scr
    // class index, or if status.scr/class-constants/movement-speed data
    // isn't loaded for that class (see ResolveStatusClassIndex in
    // RawStatCalculator.cpp).
    static std::optional<CharacterDerivedStats> Calculate(const Character& character, const StatusTable& statusRates);
};
