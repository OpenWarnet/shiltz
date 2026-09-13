#pragma once

struct CharacterDerivedStats;
struct Item;
struct ItemRecord;

// Computes an item's full derived-stat contribution: `record`'s own flat
// `*_bonus` columns, plus this item's magic-option roll and refine ("+N")
// growth against `record` (see the private helpers below). Pure function of
// the item's own state plus `record` -- computed fresh on every call, not
// cached (see ItemStatCalculator.cpp). See RawStatCalculator/
// EquipmentStatCalculator for the other two calculators in the pipeline --
// EquipmentStatCalculator sums this across every equipped item.
class ItemStatCalculator
{
public:
    static CharacterDerivedStats Calculate(const Item& item, const ItemRecord& record);

private:
    // The item's magic-option roll contribution against `record`'s matching
    // `*_scale` columns -- decodes option_bits into a per-gate deviation
    // from baseline tier 2 (the inverse of RollOptionBits, handlers/
    // ItemConfirmNpc.cpp), each times its gate's scale column, in the same
    // damage/magic/defense/attack_speed/accuracy/critical_rate/
    // evasion_rate/movement_speed/hp_percent/ap_percent order
    // RollOptionBits packs option_bits in.
    static CharacterDerivedStats CalculateOptionContribution(const Item& item, const ItemRecord& record);

    // The item's refine ("+N") growth at its current refine_level, against
    // `record`'s refine_group/refine_*_scale columns -- see
    // ItemStatCalculator.cpp for the per-(refine_group, level) curve.
    // refine_level == 0 contributes nothing.
    static CharacterDerivedStats CalculateRefineContribution(const Item& item, const ItemRecord& record);
};
