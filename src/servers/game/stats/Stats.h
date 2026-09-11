#pragma once

struct Character;
class ItemTable;
class SetOptionTable;
class StatusTable;

// Recomputes character.stats.derived (CharacterDerivedStats) by summing each stat
// source's contribution: RawStatCalculator (job/level/raw-stat baseline) and
// EquipmentStatCalculator (equipped items' bonuses, magic options, refine
// growth, set bonuses), combined via operator+(CharacterDerivedStats). A future
// SkillBuffCalculator belongs here the same way. Purely an in-memory cache --
// everything it reads is already persisted or static data, so it's fully
// reproducible and never gets its own DB column. Call after any trigger that
// could change an input: today that's a successful CG_CHAR_STATUS_UP
// (handlers/CharStatus.cpp) and character load (handlers/Session.cpp's
// CG_ENTER) -- structurally, later triggers like level-up or an equipment
// change belong here too.
//
// No-ops (leaves character.stats.derived untouched) if character.job_id can't be
// resolved to a status.scr class index, or if status.scr wasn't loaded for
// that class -- see ResolveStatusClassIndex in RawStatCalculator.cpp.
void RecalculateDerivedStats(Character& character, const ItemTable& items, const SetOptionTable& setOptions,
                              const StatusTable& statusRates);
