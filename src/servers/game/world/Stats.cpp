#include "Stats.h"

#include "enums/JobId.h"
#include "world/Player.h"
#include "world/World.h"

#include <cstdint>
#include <iostream>
#include <optional>

namespace
{
// No promotion system exists in this server yet -- nothing sets
// job_id after character creation (see login/handlers/Character.cpp)
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

// Not in status.scr -- these live only in the server binary or, for
// job ids the binary doesn't cover, a live client. Hardcoded here.
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
// and not level-dependent. Rows marked "not live-tested" are inferred
// from a shared baseline in the binary and may be wrong.
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

// trunc() toward zero, matching every formula below -- these values
// are always non-negative, so a plain cast through int64_t does the
// right thing (never use std::round here).
std::uint32_t TruncU32(double value)
{
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
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
    // kDamageBase is a real, confirmed constant (a bare-hands/no-weapon
    // floor), not a placeholder.
    constexpr double kDamageBase = 5;
    constexpr double kSkillBonus = 0;
    constexpr double kPctBuffA = 0;
    constexpr double kPctBuffB = 0;
    constexpr double kFlatBuff = 0;

    const double strTerm = static_cast<double>(TruncU32(str * *damageRate));
    double damage =
        kDamageBase + kSkillBonus + strTerm * (1 + kPctBuffA / 100 + kPctBuffB / 100) + kFlatBuff;

    // Clown's AGI-scaled damage bonus, from kClownDamageBonusBlock.
    if (clownDamageBonusRate)
        damage += static_cast<double>(TruncU32(dex * *clownDamageBonusRate));

    derived.damage = TruncU32(damage);

    derived.magic = TruncU32(intel * *magicRate);

    derived.critical = TruncU32(dex * *criticalRate);

    // kDefenseBase mirrors kDamageBase above (an unarmored floor) --
    // also a real, confirmed constant.
    constexpr double kDefenseBase = 5;

    derived.defense = TruncU32(con * *defenseRate + kDefenseBase);

    derived.accuracy = TruncU32(dex * *accuracyRate + level * constants.accuracy_level_factor +
                                constants.accuracy_base_bonus);

    derived.evasion = TruncU32(sen * *evasionRate + level * constants.evasion_level_factor +
                               constants.evasion_base_bonus);

    // HPBase/BuffBonus come from equipment/buffs -- none exist yet.
    constexpr double kHpBase = 0;
    constexpr double kHpBuffBonus = 0;
    derived.max_hp = TruncU32(kHpBase) + TruncU32(kHpBuffBonus) +
                     TruncU32(con * *maxHpRate + level * 20 + constants.max_hp_base_bonus);

    // MaxAP's Level*5+30 term is fully universal -- no per-class
    // variation beyond APRate.
    derived.max_ap = TruncU32(men * *apRate + level * 5 + 30);

    derived.movement_speed = *movementSpeedOffset;

    // attack_speed/damage_increase/damage_decrease have no confirmed
    // raw-stat formula (job base + equipment + buffs only, none of
    // which exist here) -- left untouched.

    std::cout << "Recalculated derived stats for " << player.name << ": " << "\n"
              << "* max_hp=" << derived.max_hp << "\n"
              << "* max_ap=" << derived.max_ap << "\n"
              << "* damage=" << derived.damage << "\n"
              << "* magic=" << derived.magic << "\n"
              << "* defense=" << derived.defense << "\n"
              << "* accuracy=" << derived.accuracy << "\n"
              << "* evasion=" << derived.evasion << "\n"
              << "* critical=" << derived.critical << "\n"
              << "* movement_speed=" << derived.movement_speed << "\n";
}
