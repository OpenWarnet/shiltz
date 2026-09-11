#pragma once

#include "world/Character.h" // CharacterSkill

#include <cstdint>
#include <vector>

class IDatabase;

// Owns every DB access for a character's skill economy: learned skill
// levels (`character_skill`, one row per skill_id) and unallocated skill/
// enforce points (`character.unallocated_sp`/`unallocated_ep`). The points
// columns live on `character` rather than `character_skill`, but are always
// read and written alongside skill levels by their callers (see
// handlers/CharSkillUp.cpp, handlers/LevelUp.cpp) -- grouped by feature
// rather than by table, same as BankRepository spans bank_accounts/
// bank_items.
namespace SkillRepository
{
    // For Character::LoadFromDB.
    std::vector<CharacterSkill> LoadSkillLevels(IDatabase& db, std::int64_t characterId);

    // Upserts every entry in `skills` into `character_skill`. Persisting the
    // whole list rather than a single changed skill mirrors how a
    // CG_CHAR_SKILL_UP_EX request can raise several skills at once --
    // re-upserting an unchanged skill is harmless.
    void SaveSkillLevels(IDatabase& db, std::int64_t characterId, const std::vector<CharacterSkill>& skills);

    void SaveSkillPoints(IDatabase& db, std::int64_t characterId, std::uint32_t unallocatedSp,
                          std::uint32_t unallocatedEp);
} // namespace SkillRepository
