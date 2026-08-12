#pragma once

struct Player;
class World;

// Recomputes player.stats.derived (PlayerDerivedStats) from player's
// current raw stats, level, and job_id, using World's status.scr rate
// tables plus the per-class constants in Stats.cpp, then adds equipment-
// derived contributions from each equipped item's ItemRecord `*_bonus`
// columns (see CalculateEquipmentDerivedStats in Stats.cpp). Purely an
// in-memory cache -- everything it reads is already persisted or static
// data, so it's fully reproducible and never gets its own DB column. Call
// after any trigger that could change an input: today that's a successful
// CG_CHAR_STATUS_UP (handlers/CharStatus.cpp) and character load
// (handlers/Session.cpp's CG_ENTER) -- structurally, later triggers like
// level-up or an equipment change belong here too.
//
// No-ops (leaves player.stats.derived untouched) if player.job_id can't be
// resolved to a status.scr class index, or if status.scr wasn't loaded for
// that class -- see ResolveStatusClassIndex in Stats.cpp.
void RecalculateDerivedStats(Player& player, const World& world);
