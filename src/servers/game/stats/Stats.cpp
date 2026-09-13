#include "Stats.h"

#include "EquipmentStatCalculator.h"
#include "RawStatCalculator.h"
#include "tables/GameData.h"
#include "world/Character.h"

#include <cstdint>
#include <optional>

void RecalculateDerivedStats(Character& character, const GameData& data)
{
    const std::optional<CharacterDerivedStats> raw = RawStatCalculator::Calculate(character, data.statusRates);
    if (!raw)
        return;

    const CharacterDerivedStats equipment =
        EquipmentStatCalculator::Calculate(character, data.items, data.setOptions);

    CharacterDerivedStats total = *raw + equipment;

    // hp_percent_bonus/ap_percent_bonus scale the combined total as one
    // final step, rather than adding directly like every other field -- see
    // EquipmentStatCalculator.h.
    total.max_hp += static_cast<std::int32_t>(static_cast<std::int64_t>(total.max_hp) *
                                               total.hp_percent_bonus / 100);
    total.max_ap += static_cast<std::int32_t>(static_cast<std::int64_t>(total.max_ap) *
                                               total.ap_percent_bonus / 100);

    character.stats.derived = total;
}
