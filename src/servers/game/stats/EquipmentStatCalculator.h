#pragma once

struct Character;
struct CharacterDerivedStats;
class ItemTable;
class SetOptionTable;

// Aggregates equipment's contribution to CharacterDerivedStats: sums each
// equipped item's own CalculateDerivedStats() (its flat ItemRecord
// `*_bonus` columns, magic-option roll, and refine ("+N") growth -- see
// world/Item.h, which owns that per-item math since it depends only on the
// item's own state, not the character or the rest of the equipped set), plus
// any completed-set bonus (set_opt.scr), which stays here rather than on
// Item since it's inherently cross-item. See RawStatCalculator for the
// other half of the pipeline -- RecalculateDerivedStats (Stats.cpp)
// combines both via operator+(CharacterDerivedStats).
//
// hp_percent_bonus/ap_percent_bonus carry equipment's HP%/AP% contribution.
// Unlike every other field, they don't add directly onto max_hp/max_ap --
// RecalculateDerivedStats applies them as one final multiplicative step
// after summing every calculator's output, since percent-of-total isn't
// expressible as a plain per-field add.
class EquipmentStatCalculator
{
public:
    static CharacterDerivedStats Calculate(const Character& character, const ItemTable& items,
                                          const SetOptionTable& setOptions);
};
