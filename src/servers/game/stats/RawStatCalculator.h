#pragma once

#include <optional>

struct Player;
struct PlayerDerivedStats;
class StatusTable;

// Computes the job/level/raw-stat baseline of PlayerDerivedStats: no
// equipment, no buffs. Uses StatusTable's status.scr rate tables plus the
// per-class constants in RawStatCalculator.cpp. See EquipmentStatCalculator for
// the other half of the pipeline -- RecalculateDerivedStats (Stats.cpp)
// combines both via operator+(PlayerDerivedStats).
class RawStatCalculator
{
public:
    // Returns nullopt if player.job_id can't be resolved to a status.scr
    // class index, or if status.scr/class-constants/movement-speed data
    // isn't loaded for that class (see ResolveStatusClassIndex in
    // RawStatCalculator.cpp).
    static std::optional<PlayerDerivedStats> Calculate(const Player& player, const StatusTable& statusRates);
};
