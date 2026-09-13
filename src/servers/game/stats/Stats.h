#pragma once

struct Character;
class GameData;

// Recomputes character.stats.derived by summing each stat source's contribution:
// RawStatCalculator (job/level/raw-stat baseline) and EquipmentStatCalculator
// (equipped items' bonuses, magic options, refine growth, set bonuses),
// combined via operator+(CharacterDerivedStats). A future SkillBuffCalculator
// belongs here the same way. Purely an in-memory cache -- everything it reads
// is already persisted or static data, so it's fully reproducible and never
// gets its own DB column.
//
// Character has no GameData dependency of its own (see CharacterStats::dirty),
// so this is a free function rather than a Character method -- its only caller
// is Map::RecalculateDirtyStats, which sweeps every dirty player once per tick
// rather than requiring every mutation site to call this directly.
//
// No-op (leaves character.stats.derived untouched) if character.job_id can't be
// resolved to a status.scr class index, or if status.scr wasn't loaded for
// that class -- see ResolveStatusClassIndex in RawStatCalculator.cpp.
void RecalculateDerivedStats(Character& character, const GameData& data);
