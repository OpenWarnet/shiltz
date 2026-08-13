#include "Stats.h"

#include "EquipmentStatCalculator.h"
#include "RawStatCalculator.h"
#include "world/Player.h"

#include <cstdint>
#include <iostream>
#include <optional>

void RecalculateDerivedStats(Player& player, const ItemTable& items, const SetOptionTable& setOptions,
                              const StatusTable& statusRates)
{
    const std::optional<PlayerDerivedStats> raw = RawStatCalculator::Calculate(player, statusRates);
    if (!raw)
        return;

    const PlayerDerivedStats equipment = EquipmentStatCalculator::Calculate(player, items, setOptions);

    PlayerDerivedStats total = *raw + equipment;

    // hp_percent_bonus/ap_percent_bonus scale the combined total as one
    // final step, rather than adding directly like every other field -- see
    // EquipmentStatCalculator.h.
    total.max_hp += static_cast<std::int32_t>(static_cast<std::int64_t>(total.max_hp) *
                                               total.hp_percent_bonus / 100);
    total.max_ap += static_cast<std::int32_t>(static_cast<std::int64_t>(total.max_ap) *
                                               total.ap_percent_bonus / 100);

    player.stats.derived = total;

    const PlayerDerivedStats& derived = player.stats.derived;
    std::cout << "Recalculated derived stats for " << player.name << ": " << "\n"
              << "* max_hp=" << derived.max_hp << "\n"
              << "* max_ap=" << derived.max_ap << "\n"
              << "* damage=" << derived.damage << "\n"
              << "* magic=" << derived.magic << "\n"
              << "* defense=" << derived.defense << "\n"
              << "* accuracy=" << derived.accuracy << "\n"
              << "* evasion=" << derived.evasion << "\n"
              << "* critical=" << derived.critical << "\n"
              << "* attack_speed=" << derived.attack_speed << "\n"
              << "* movement_speed=" << derived.movement_speed << "\n"
              << "* damage_dealt_increase_percent=" << derived.damage_dealt_increase_percent << "\n"
              << "* damage_taken_decrease_percent=" << derived.damage_taken_decrease_percent << "\n";
}
