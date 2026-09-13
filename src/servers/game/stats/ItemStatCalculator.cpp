#include "ItemStatCalculator.h"

#include "enums/RefineGroup.h"
#include "parser/ItemScr.h"
#include "world/Character.h"
#include "world/Item.h"

#include <array>
#include <cstdint>
#include <optional>

namespace
{
constexpr int kOptionGateCount = 10;
constexpr int kOptionBaselineTier = 2;

// Decodes an Item::option_bits into a per-gate deviation from
// baseline tier 2 -- the inverse of RollOptionBits (handlers/
// ItemConfirmNpc.cpp). optionBits == 0 short-circuits to "every gate at
// baseline" rather than being decoded gate-by-gate, since 0 is both the
// column default for a never-appraised item and RollOptionBits' own
// canonical result for "every gate rolled baseline" -- decoding it
// literally would read every gate as tier 0 (a large negative roll)
// instead.
std::array<std::int32_t, kOptionGateCount> DecodeOptionDeviations(std::uint32_t optionBits)
{
    std::array<std::int32_t, kOptionGateCount> deviations{};
    if (optionBits == 0)
        return deviations;

    for (int gate = 0; gate < kOptionGateCount; ++gate)
    {
        const int tier = static_cast<int>((optionBits >> (gate * 3)) & 0b111);
        deviations[static_cast<std::size_t>(gate)] = tier - kOptionBaselineTier;
    }

    return deviations;
}

// Refine ("+N") growth: growth(stat, level) = a curve shared by every item
// with the same ItemRecord::refine_group (see enums/RefineGroup.h --
// Weapon, or Armor4/Armor6 sharing one curve), multiplied by that item's
// own refine_damage_scale/refine_magic_scale/refine_defense_scale.
// damage_dealt_increase_percent is a separate flat per-group curve
// (RefineGroup::Weapon only), gated on the item having a nonzero base
// damage_dealt_increase_percent_bonus.
struct RefineCurvePoint
{
    std::uint32_t refine_level;
    std::int64_t points;
};

// RefineGroup::Armor4/Armor6 -- per unit of refine_damage_scale/
// refine_magic_scale/refine_defense_scale.
constexpr RefineCurvePoint kRefineCurveArmor[] = {
    {0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 5}, {5, 7}, {6, 9},
    {7, 12}, {8, 15}, {9, 18}, {10, 22}, {11, 26}, {12, 30},
};

// RefineGroup::Weapon -- per unit of refine_damage_scale/refine_magic_scale.
constexpr RefineCurvePoint kRefineCurveWeapon[] = {
    {0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 5}, {5, 7}, {6, 9},
    {7, 13}, {8, 17}, {9, 21}, {10, 27}, {11, 33}, {12, 39},
};

// Weapon damage_dealt_increase_percent growth -- flat, NOT multiplied by
// any column (see comment above).
constexpr RefineCurvePoint kRefineDamagePercentCurveWeapon[] = {
    {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 1}, {5, 2}, {6, 3},
    {7, 4}, {8, 5}, {9, 6}, {10, 7}, {11, 8}, {12, 9},
};

std::optional<std::int64_t> FindCurvePoints(const RefineCurvePoint* curve, std::size_t count,
                                             std::uint32_t refineLevel)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        if (curve[i].refine_level == refineLevel)
            return curve[i].points;
    }

    return std::nullopt;
}
} // namespace

CharacterDerivedStats ItemStatCalculator::CalculateOptionContribution(const Item& item, const ItemRecord& record)
{
    const std::array<std::int32_t, kOptionGateCount> deviations = DecodeOptionDeviations(item.option_bits);

    CharacterDerivedStats derived;
    derived.damage = static_cast<std::int32_t>(deviations[0] * record.damage_scale);
    derived.magic = static_cast<std::int32_t>(deviations[1] * record.magic_power_scale);
    derived.defense = static_cast<std::int32_t>(deviations[2] * record.defense_scale);
    derived.attack_speed = static_cast<std::int32_t>(deviations[3] * record.attack_speed_scale);
    derived.accuracy = static_cast<std::int32_t>(deviations[4] * record.accuracy_scale);
    derived.critical = static_cast<std::int32_t>(deviations[5] * record.critical_rate_scale);
    derived.evasion = static_cast<std::int32_t>(deviations[6] * record.evasion_rate_scale);
    derived.movement_speed = static_cast<std::int32_t>(deviations[7] * record.movement_speed_scale);
    derived.hp_percent_bonus = static_cast<std::int32_t>(deviations[8] * record.hp_percent_scale);
    derived.ap_percent_bonus = static_cast<std::int32_t>(deviations[9] * record.ap_percent_scale);

    return derived;
}

CharacterDerivedStats ItemStatCalculator::CalculateRefineContribution(const Item& item, const ItemRecord& record)
{
    CharacterDerivedStats derived;
    if (item.refine_level == 0)
        return derived;

    const auto refineGroup = static_cast<RefineGroup>(record.refine_group);

    const RefineCurvePoint* curve = nullptr;
    std::size_t curveSize = 0;
    if (refineGroup == RefineGroup::Armor4 || refineGroup == RefineGroup::Armor6)
    {
        curve = kRefineCurveArmor;
        curveSize = std::size(kRefineCurveArmor);
    }
    else if (refineGroup == RefineGroup::Weapon)
    {
        curve = kRefineCurveWeapon;
        curveSize = std::size(kRefineCurveWeapon);
    }

    if (curve)
    {
        if (const std::optional<std::int64_t> points = FindCurvePoints(curve, curveSize, item.refine_level))
        {
            derived.damage = static_cast<std::int32_t>(*points * record.refine_damage_scale);
            derived.magic = static_cast<std::int32_t>(*points * record.refine_magic_scale);
            derived.defense = static_cast<std::int32_t>(*points * record.refine_defense_scale);
        }
    }

    if (refineGroup == RefineGroup::Weapon && record.damage_dealt_increase_percent_bonus != 0)
    {
        if (const std::optional<std::int64_t> points = FindCurvePoints(
                kRefineDamagePercentCurveWeapon, std::size(kRefineDamagePercentCurveWeapon), item.refine_level))
        {
            derived.damage_dealt_increase_percent = static_cast<std::int32_t>(*points);
        }
    }

    return derived;
}

CharacterDerivedStats ItemStatCalculator::Calculate(const Item& item, const ItemRecord& record)
{
    CharacterDerivedStats flat;
    flat.damage = static_cast<std::int32_t>(record.damage_bonus);
    flat.damage_dealt_increase_percent =
        static_cast<std::int32_t>(record.damage_dealt_increase_percent_bonus);
    flat.magic = static_cast<std::int32_t>(record.magic_power_bonus);
    flat.defense = static_cast<std::int32_t>(record.defense_bonus);
    flat.damage_taken_decrease_percent =
        static_cast<std::int32_t>(record.damage_taken_decrease_percent_bonus);
    flat.attack_speed = static_cast<std::int32_t>(record.attack_speed_bonus);
    flat.accuracy = static_cast<std::int32_t>(record.accuracy_bonus);
    flat.critical = static_cast<std::int32_t>(record.critical_rate_bonus);
    flat.evasion = static_cast<std::int32_t>(record.evasion_rate_bonus);
    flat.movement_speed = static_cast<std::int32_t>(record.movement_speed_bonus);
    flat.max_hp = static_cast<std::int32_t>(record.hp_bonus);
    flat.max_ap = static_cast<std::int32_t>(record.ap_bonus);

    return flat + CalculateOptionContribution(item, record) + CalculateRefineContribution(item, record);
}
