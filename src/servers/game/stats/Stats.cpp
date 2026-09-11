#include "Stats.h"

#include "EquipmentStatCalculator.h"
#include "RawStatCalculator.h"
#include "world/Character.h"

#include <cstdint>
#include <iostream>
#include <optional>

void RecalculateDerivedStats(Character& character, const ItemTable& items, const SetOptionTable& setOptions,
                              const StatusTable& statusRates)
{
    const std::optional<CharacterDerivedStats> raw = RawStatCalculator::Calculate(character, statusRates);
    if (!raw)
        return;

    const CharacterDerivedStats equipment = EquipmentStatCalculator::Calculate(character, items, setOptions);

    CharacterDerivedStats total = *raw + equipment;

    // hp_percent_bonus/ap_percent_bonus scale the combined total as one
    // final step, rather than adding directly like every other field -- see
    // EquipmentStatCalculator.h.
    total.max_hp += static_cast<std::int32_t>(static_cast<std::int64_t>(total.max_hp) *
                                               total.hp_percent_bonus / 100);
    total.max_ap += static_cast<std::int32_t>(static_cast<std::int64_t>(total.max_ap) *
                                               total.ap_percent_bonus / 100);

    character.stats.derived = total;

    const CharacterDerivedStats& derived = character.stats.derived;
    std::cout << "Recalculated derived stats for " << character.name << ": " << "\n"
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
