#include "Stats.h"

#include "enums/JobId.h"
#include "world/Player.h"
#include "world/World.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <unordered_map>

namespace
{
// Collapses tiered job ids (e.g. WarriorTier2/3) down to their base class,
// since status.scr and the tables below are keyed per base class only.
// No promotion system exists yet, so job_id never actually holds a tiered
// value today, but the mapping is here for when it does.
std::optional<JobId> ResolveStatusClassIndex(std::uint32_t jobId)
{
    JobId id = static_cast<JobId>(jobId);

    switch (id)
    {
    case JobId::WarriorTier2:
    case JobId::WarriorTier3:
        id = JobId::Warrior;
        break;
    case JobId::KnightTier2:
    case JobId::KnightTier3:
        id = JobId::Knight;
        break;
    case JobId::ClownTier2:
    case JobId::ClownTier3:
        id = JobId::Clown;
        break;
    case JobId::MageTier2:
    case JobId::MageTier3:
        id = JobId::Mage;
        break;
    case JobId::PriestTier2:
    case JobId::PriestTier3:
        id = JobId::Priest;
        break;
    case JobId::CraftsmanTier2:
    case JobId::CraftsmanTier3:
        id = JobId::Craftsman;
        break;
    case JobId::HunterTier2:
    case JobId::HunterTier3:
        id = JobId::Hunter;
        break;
    case JobId::CookTier2:
    case JobId::CookTier3:
        id = JobId::Cook;
        break;
    default:
        break;
    }

    if (id <= JobId::Hunter || id == JobId::Cook)
        return id;

    return std::nullopt;
}

// Not in status.scr; hardcoded per job id here.
struct ClassConstants
{
    double accuracy_level_factor;
    double accuracy_base_bonus;
    double evasion_level_factor;
    double evasion_base_bonus;
    double max_hp_base_bonus;
};

struct ClassConstantsEntry
{
    JobId job_id;
    ClassConstants constants;
};

constexpr ClassConstantsEntry kClassConstants[] = {
    {JobId::Beginner, {1.5, 40, 2.0, 10, 50}},
    {JobId::Warrior, {1.3, 20, 1.8, 10, 250}},
    {JobId::Knight, {1.15, 30, 1.8, 0, 300}},
    {JobId::Clown, {1.15, 40, 1.8, 20, 170}},
    {JobId::Mage, {1.9, 40, 1.8, 15, 150}},
    {JobId::Priest, {1.9, 30, 1.8, 10, 200}},
    {JobId::Craftsman, {1.9, 20, 1.2, 0, 250}},
    {JobId::GameMaster, {1.5, 40, 2.0, 10, 50}},
    {JobId::Vagabond, {1.5, 40, 2.0, 10, 50}},
    {JobId::Hunter, {1.1, 40, 1.8, 18, 160}},
    {JobId::Cook, {1.5, 20, 1.5, 10, 230}},
};

const ClassConstants* FindClassConstants(JobId jobId)
{
    for (const auto& entry : kClassConstants)
    {
        if (entry.job_id == jobId)
            return &entry.constants;
    }

    return nullptr;
}

struct MovementSpeedEntry
{
    JobId job_id;
    std::int32_t offset;
};

// A flat per-class integer offset -- not a rate against any raw stat,
// and not level-dependent.
constexpr MovementSpeedEntry kMovementSpeedOffset[] = {
    {JobId::Beginner, 0},
    {JobId::Warrior, 9},
    {JobId::Knight, -5},
    {JobId::Clown, 6},
    {JobId::Mage, -15},
    {JobId::Priest, 3},
    {JobId::Craftsman, 0},
    {JobId::GameMaster, 0},
    {JobId::Vagabond, 0},
    {JobId::Hunter, 6},
    {JobId::Cook, 6},
};

std::optional<std::int32_t> FindMovementSpeedOffset(JobId jobId)
{
    for (const auto& entry : kMovementSpeedOffset)
    {
        if (entry.job_id == jobId)
            return entry.offset;
    }

    return std::nullopt;
}

// trunc() toward zero, matching every formula below (never use std::round
// here). The raw-stat formulas always produce non-negative values, but the
// equipment pass added later in RecalculateDerivedStats can push the final
// summed value negative, hence returning a signed type.
std::int32_t TruncI32(double value)
{
    return static_cast<std::int32_t>(static_cast<std::int64_t>(value));
}

// Mirrors PlayerDerivedStats' additive fields, but signed and equipment-
// only (see AddEquipmentContribution for why a negative total is
// possible). Fed by three sources per equipped item -- ItemRecord's flat
// `*_bonus` fields, the option_bits magic-option roll (see
// AddOptionContribution), refine ("+N") growth (see AddRefineContribution)
// -- plus a completed-set bonus from set_opt.scr (see AddSetOptionRecord).
//
// hp_percent/ap_percent scale the raw-derived max_hp/max_ap rather than
// adding a flat amount, so they're kept apart from hp_flat/ap_flat and
// applied separately once that raw-derived base is known (see
// RecalculateDerivedStats); how flat and percent combine when both are
// present on the same stat is unconfirmed.
struct EquipmentDerivedStats
{
    std::int64_t damage = 0;
    std::int64_t damage_dealt_increase_percent = 0;
    std::int64_t magic = 0;
    std::int64_t defense = 0;
    std::int64_t damage_taken_decrease_percent = 0;
    std::int64_t attack_speed = 0;
    std::int64_t accuracy = 0;
    std::int64_t critical = 0;
    std::int64_t evasion = 0;
    std::int64_t movement_speed = 0;
    std::int64_t hp_flat = 0;
    std::int64_t ap_flat = 0;
    std::int64_t hp_percent = 0;
    std::int64_t ap_percent = 0;
};

constexpr int kOptionGateCount = 10;
constexpr int kOptionBaselineTier = 2;

// Decodes a PlayerEquipmentItem::option_bits into a per-gate deviation from
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

// Adds one equipped item's option-roll contribution -- each gate's
// deviation from baseline times that item's matching ItemRecord `*_scale`
// column, in the same damage/magic/defense/attack_speed/accuracy/
// critical_rate/evasion_rate/movement_speed/hp_percent/ap_percent order as
// both RollOptionBits' gates and ItemRecord's `*_scale` fields.
void AddOptionContribution(EquipmentDerivedStats& equipment, const ItemRecord& itemRecord,
                            std::uint32_t optionBits)
{
    const std::array<std::int32_t, kOptionGateCount> deviations = DecodeOptionDeviations(optionBits);

    equipment.damage += deviations[0] * itemRecord.damage_scale;
    equipment.magic += deviations[1] * itemRecord.magic_power_scale;
    equipment.defense += deviations[2] * itemRecord.defense_scale;
    equipment.attack_speed += deviations[3] * itemRecord.attack_speed_scale;
    equipment.accuracy += deviations[4] * itemRecord.accuracy_scale;
    equipment.critical += deviations[5] * itemRecord.critical_rate_scale;
    equipment.evasion += deviations[6] * itemRecord.evasion_rate_scale;
    equipment.movement_speed += deviations[7] * itemRecord.movement_speed_scale;
    equipment.hp_percent += deviations[8] * itemRecord.hp_percent_scale;
    equipment.ap_percent += deviations[9] * itemRecord.ap_percent_scale;
}

// Refine ("+N") growth: growth(stat, level) = a curve shared by every item
// with the same ItemRecord::refine_group (2 = weapon, 4/6 = armor, sharing
// one curve), multiplied by that item's own refine_damage_scale/
// refine_magic_scale/refine_defense_scale. damage_dealt_increase_percent is
// a separate flat per-group curve (weapon only), gated on the item having a
// nonzero base damage_dealt_increase_percent_bonus.
struct RefineCurvePoint
{
    std::uint32_t refine_level;
    std::int64_t points;
};

// refine_group 4 and 6 (armor) -- per unit of refine_damage_scale/
// refine_magic_scale/refine_defense_scale.
constexpr RefineCurvePoint kRefineCurveArmor[] = {
    {0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 5}, {5, 7}, {6, 9},
    {7, 12}, {8, 15}, {9, 18}, {10, 22}, {11, 26}, {12, 30},
};

// refine_group 2 (weapon) -- per unit of refine_damage_scale/refine_magic_scale.
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

// Adds one equipped item's refine ("+N") growth at its exact current
// level. No curve for this item's refine_group, or no point at this exact
// level, contributes nothing rather than an interpolated/extrapolated
// guess.
void AddRefineContribution(EquipmentDerivedStats& equipment, const ItemRecord& itemRecord,
                            std::uint32_t refineLevel)
{
    if (refineLevel == 0)
        return;

    const RefineCurvePoint* curve = nullptr;
    std::size_t curveSize = 0;
    if (itemRecord.refine_group == 4 || itemRecord.refine_group == 6)
    {
        curve = kRefineCurveArmor;
        curveSize = std::size(kRefineCurveArmor);
    }
    else if (itemRecord.refine_group == 2)
    {
        curve = kRefineCurveWeapon;
        curveSize = std::size(kRefineCurveWeapon);
    }

    if (curve)
    {
        if (const std::optional<std::int64_t> points = FindCurvePoints(curve, curveSize, refineLevel))
        {
            equipment.damage += *points * itemRecord.refine_damage_scale;
            equipment.magic += *points * itemRecord.refine_magic_scale;
            equipment.defense += *points * itemRecord.refine_defense_scale;
        }
    }

    if (itemRecord.refine_group == 2 && itemRecord.damage_dealt_increase_percent_bonus != 0)
    {
        if (const std::optional<std::int64_t> points = FindCurvePoints(
                kRefineDamagePercentCurveWeapon, std::size(kRefineDamagePercentCurveWeapon), refineLevel))
        {
            equipment.damage_dealt_increase_percent += *points;
        }
    }
}

void AddSetOptionRecord(EquipmentDerivedStats& equipment, const SetOptionRecord& set)
{
    equipment.damage += set.damage;
    equipment.magic += set.magic;
    equipment.defense += set.defense;
    equipment.attack_speed += set.attack_speed;
    equipment.accuracy += set.accuracy;
    equipment.critical += set.critical_rate;
    equipment.evasion += set.evasion;
    equipment.movement_speed += set.movement_speed;
    equipment.hp_percent += set.hp_percent;
    equipment.ap_percent += set.ap_percent;
}

EquipmentDerivedStats CalculateEquipmentDerivedStats(const Player& player, const World& world)
{
    EquipmentDerivedStats equipment;

    // Tallies how many currently-equipped items share each nonzero
    // ItemRecord::set_id, so a completed set's bonus (set_opt.scr, keyed by
    // (set_id, exact piece count worn)) can be looked up once per set
    // rather than per item.
    std::unordered_map<std::int64_t, std::int64_t> setPieceCounts;

    for (const PlayerEquipmentItem& item : player.equipment)
    {
        const ItemRecord* itemRecord = world.FindItemRecord(item.item_id);
        if (!itemRecord)
            continue; // unknown item_id -- no bonus data to apply

        // Flat stat bonus from simply wearing the item -- see ItemRecord's
        // `*_bonus` fields.
        equipment.damage += itemRecord->damage_bonus;
        equipment.damage_dealt_increase_percent += itemRecord->damage_dealt_increase_percent_bonus;
        equipment.magic += itemRecord->magic_power_bonus;
        equipment.defense += itemRecord->defense_bonus;
        equipment.damage_taken_decrease_percent += itemRecord->damage_taken_decrease_percent_bonus;
        equipment.attack_speed += itemRecord->attack_speed_bonus;
        equipment.accuracy += itemRecord->accuracy_bonus;
        equipment.critical += itemRecord->critical_rate_bonus;
        equipment.evasion += itemRecord->evasion_rate_bonus;
        equipment.movement_speed += itemRecord->movement_speed_bonus;
        equipment.hp_flat += itemRecord->hp_bonus;
        equipment.ap_flat += itemRecord->ap_bonus;

        // Magic-option roll and refine ("+N") growth on top of the flat
        // bonus above -- see AddOptionContribution/AddRefineContribution.
        AddOptionContribution(equipment, *itemRecord, item.option_bits);
        AddRefineContribution(equipment, *itemRecord, item.refine_level);

        if (itemRecord->set_id != 0)
            ++setPieceCounts[itemRecord->set_id];
    }

    for (const auto& [setId, pieceCount] : setPieceCounts)
    {
        // No partial-set fallback: set_opt.scr's row is keyed by the exact
        // piece count worn, and most (set_id, piece_count) pairs simply
        // don't have a row (e.g. set 255 only has one, for all 4 pieces).
        const SetOptionRecord* setOption = world.FindSetOptionRecord(setId, pieceCount);
        if (setOption)
            AddSetOptionRecord(equipment, *setOption);
    }

    return equipment;
}

// Adds a possibly-negative equipment contribution to a raw-derived value.
// Deliberately not clamped at zero -- a "cursed" enough loadout (an
// equipment option roll below its baseline tier) can legitimately push a
// stat negative, and PlayerDerivedStats' fields are signed to allow that.
std::int32_t AddEquipmentContribution(std::int32_t rawDerived, std::int64_t equipmentContribution)
{
    return static_cast<std::int32_t>(static_cast<std::int64_t>(rawDerived) + equipmentContribution);
}
} // namespace

void RecalculateDerivedStats(Player& player, const World& world)
{
    const std::optional<JobId> jobId = ResolveStatusClassIndex(player.job_id);
    if (!jobId)
        return;

    const double* damageRate = world.FindStatusRate(World::kDamageBlock, *jobId);
    const double* magicRate = world.FindStatusRate(World::kMagicBlock, *jobId);
    const double* accuracyRate = world.FindStatusRate(World::kAccuracyBlock, *jobId);
    const double* maxHpRate = world.FindStatusRate(World::kMaxHpBlock, *jobId);
    const double* apRate = world.FindStatusRate(World::kApBlock, *jobId);
    const double* criticalRate = world.FindStatusRate(World::kCriticalBlock, *jobId);
    const double* evasionRate = world.FindStatusRate(World::kEvasionBlock, *jobId);
    const double* defenseRate = world.FindStatusRate(World::kDefenseBlock, *jobId);

    if (!damageRate || !magicRate || !accuracyRate || !maxHpRate || !apRate || !criticalRate ||
        !evasionRate || !defenseRate)
        return; // status.scr not loaded for this job id.

    // Sparse -- only JobId::Clown has a row for this block. nullptr
    // (treated as 0) for everyone else is correct, not missing data.
    const double* clownDamageBonusRate = world.FindStatusRate(World::kClownDamageBonusBlock, *jobId);

    const ClassConstants* constantsPtr = FindClassConstants(*jobId);
    const std::optional<std::int32_t> movementSpeedOffset = FindMovementSpeedOffset(*jobId);
    if (!constantsPtr || !movementSpeedOffset)
        return; // no ClassConstants/movement-speed entry for this job id yet.

    const ClassConstants& constants = *constantsPtr;

    const double str = player.stats.raw.strength;
    const double dex = player.stats.raw.dexterity;
    const double intel = player.stats.raw.intelligence;
    const double con = player.stats.raw.constitution;
    const double men = player.stats.raw.mentality;
    const double sen = player.stats.raw.sense;
    const double level = player.level;

    PlayerDerivedStats& derived = player.stats.derived;

    // SkillBonus/PctBuffA/PctBuffB/FlatBuff come from skill-cast bonuses
    // and active buffs -- none of those systems exist yet, so they're
    // named placeholders wired to real values once those systems land.
    // kDamageBase is not a placeholder: it's a bare-hands/no-weapon floor.
    constexpr double kDamageBase = 5;
    constexpr double kSkillBonus = 0;
    constexpr double kPctBuffA = 0;
    constexpr double kPctBuffB = 0;
    constexpr double kFlatBuff = 0;

    const double strTerm = static_cast<double>(TruncI32(str * *damageRate));
    double damage =
        kDamageBase + kSkillBonus + strTerm * (1 + kPctBuffA / 100 + kPctBuffB / 100) + kFlatBuff;

    // Clown's AGI-scaled damage bonus, from kClownDamageBonusBlock.
    if (clownDamageBonusRate)
        damage += static_cast<double>(TruncI32(dex * *clownDamageBonusRate));

    derived.damage = TruncI32(damage);

    derived.magic = TruncI32(intel * *magicRate);

    derived.critical = TruncI32(dex * *criticalRate);

    // kDefenseBase mirrors kDamageBase above: an unarmored floor.
    constexpr double kDefenseBase = 5;

    derived.defense = TruncI32(con * *defenseRate + kDefenseBase);

    derived.accuracy = TruncI32(dex * *accuracyRate + level * constants.accuracy_level_factor +
                                constants.accuracy_base_bonus);

    derived.evasion = TruncI32(sen * *evasionRate + level * constants.evasion_level_factor +
                               constants.evasion_base_bonus);

    // HPBase/BuffBonus come from equipment/buffs -- none exist yet.
    constexpr double kHpBase = 0;
    constexpr double kHpBuffBonus = 0;
    derived.max_hp = TruncI32(kHpBase) + TruncI32(kHpBuffBonus) +
                     TruncI32(con * *maxHpRate + level * 20 + constants.max_hp_base_bonus);

    // MaxAP's Level*5+30 term is fully universal -- no per-class
    // variation beyond APRate.
    derived.max_ap = TruncI32(men * *apRate + level * 5 + 30);

    derived.movement_speed = *movementSpeedOffset;

    // attack_speed/damage_dealt_increase_percent/damage_taken_decrease_percent
    // have no raw-stat (job base) formula -- they're equipment-only, added
    // below.

    // Equipment pass: sums each equipped item's flat *_bonus contribution
    // plus any completed-set bonus (see CalculateEquipmentDerivedStats) on
    // top of the raw-stat-derived values computed above, rather than
    // folding equipment into that calculation directly.
    const EquipmentDerivedStats equipment = CalculateEquipmentDerivedStats(player, world);

    derived.damage = AddEquipmentContribution(derived.damage, equipment.damage);
    derived.damage_dealt_increase_percent = AddEquipmentContribution(
        derived.damage_dealt_increase_percent, equipment.damage_dealt_increase_percent);
    derived.magic = AddEquipmentContribution(derived.magic, equipment.magic);
    derived.defense = AddEquipmentContribution(derived.defense, equipment.defense);
    derived.damage_taken_decrease_percent = AddEquipmentContribution(
        derived.damage_taken_decrease_percent, equipment.damage_taken_decrease_percent);
    derived.attack_speed = AddEquipmentContribution(derived.attack_speed, equipment.attack_speed);
    derived.accuracy = AddEquipmentContribution(derived.accuracy, equipment.accuracy);
    derived.critical = AddEquipmentContribution(derived.critical, equipment.critical);
    derived.evasion = AddEquipmentContribution(derived.evasion, equipment.evasion);
    derived.movement_speed += static_cast<std::int32_t>(equipment.movement_speed);

    // hp_percent/ap_percent scale the raw-derived max_hp/max_ap rather than
    // adding a flat amount, so they're applied against the value just
    // computed above instead of being summed as a flat field; hp_flat/
    // ap_flat are added on top of that (see EquipmentDerivedStats for why
    // that ordering, not the other way around, is the current best guess).
    const std::int64_t hpBonus =
        static_cast<std::int64_t>(derived.max_hp) * equipment.hp_percent / 100;
    derived.max_hp = AddEquipmentContribution(derived.max_hp, hpBonus + equipment.hp_flat);

    const std::int64_t apBonus =
        static_cast<std::int64_t>(derived.max_ap) * equipment.ap_percent / 100;
    derived.max_ap = AddEquipmentContribution(derived.max_ap, apBonus + equipment.ap_flat);

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
