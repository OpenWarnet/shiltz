#include "EquipmentStatCalculator.h"

#include "tables/ItemTable.h"
#include "tables/SetOptionTable.h"
#include "world/Character.h"

#include <cstdint>
#include <unordered_map>

namespace
{
CharacterDerivedStats SetOptionRecordToDerivedStats(const SetOptionRecord& set)
{
    CharacterDerivedStats derived;
    derived.damage = static_cast<std::int32_t>(set.damage);
    derived.magic = static_cast<std::int32_t>(set.magic);
    derived.defense = static_cast<std::int32_t>(set.defense);
    derived.attack_speed = static_cast<std::int32_t>(set.attack_speed);
    derived.accuracy = static_cast<std::int32_t>(set.accuracy);
    derived.critical = static_cast<std::int32_t>(set.critical_rate);
    derived.evasion = static_cast<std::int32_t>(set.evasion);
    derived.movement_speed = static_cast<std::int32_t>(set.movement_speed);
    derived.hp_percent_bonus = static_cast<std::int32_t>(set.hp_percent);
    derived.ap_percent_bonus = static_cast<std::int32_t>(set.ap_percent);
    return derived;
}
} // namespace

CharacterDerivedStats EquipmentStatCalculator::Calculate(const Character& character, const ItemTable& items,
                                                        const SetOptionTable& setOptions)
{
    CharacterDerivedStats total;

    std::unordered_map<std::int64_t, std::int64_t> setPieceCounts;

    for (const CharacterEquipmentItem& equipped : character.equipment)
    {
        const Item& item = equipped.item;
        const ItemRecord* itemRecord = items.Find(item.item_id);
        if (!itemRecord)
            continue; // unknown item_id -- no bonus data to apply

        total = total + item.CalculateDerivedStats(*itemRecord);

        if (itemRecord->set_id != 0)
            ++setPieceCounts[itemRecord->set_id];
    }

    for (const auto& [setId, pieceCount] : setPieceCounts)
    {
        const SetOptionRecord* setOption = setOptions.Find(setId, pieceCount);
        if (setOption)
            total = total + SetOptionRecordToDerivedStats(*setOption);
    }

    return total;
}
