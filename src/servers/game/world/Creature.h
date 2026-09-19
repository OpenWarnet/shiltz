#pragma once

#include "parser/MonsterScr.h"
#include "world/Combat.h"
#include "world/ai/CreatureAi.h"
#include "world/Grid.h"
#include "world/Placement.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

class MonsterTable;
class EventBus;
class Map;
struct Player;

// Per-instance origin and respawn state for a monster loaded from
// MonsterSpawnScr. NPCs have no CreatureSpawn because NpcScr places them directly.
struct CreatureSpawn
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t direction = 0;
    std::optional<std::chrono::milliseconds> time_until_respawn;

    std::pair<std::uint32_t, std::uint32_t> RollPosition(std::uint32_t radius) const;

private:
    static std::uint32_t RollCoordinate(std::uint32_t anchor, std::uint32_t radius);
};

// Where a Creature came from -- an npcNN.scr static entity (dialogue/shop/
// warp/gacha, "category == 3" in monster.scr terms) or an mNN.scr monster
// spawn. Kept as a tag on one shared struct rather than two separate types
// since both are placed on the map the same way (a template id at a fixed
// position), and a caller that just wants "what's near me" doesn't care
// which it's looking at.
enum class CreatureKind : std::uint8_t
{
    Npc,
    Monster,
};

// A single spawned creature on a Map -- one instance from either an
// npcNN.scr NpcInstance (kind == Npc) or an mNN.scr MonsterSpawnInstance
// (kind == Monster). `monster_template` refers to the immutable monster.scr
// row owned by GameData; Creature only owns per-instance state such as
// placement and current HP. `instance_id` has nothing to do with the .scr
// data itself -- it's assigned at spawn from EntityIdGenerator::Next()
// (world/common/EntityIdGenerator.h) to identify this one spawned instance
// uniquely across the whole World, the way GC_CRT_LOAD's own entity id does
// on the wire.
struct Creature
{
    std::uint32_t instance_id = 0;
    CreatureKind kind = CreatureKind::Monster;
    std::reference_wrapper<const MonsterRecord> monster_template;
    Placement placement;

    // Current HP; starts at the monster.scr template's max at spawn.
    std::int64_t hp = 0;

    // Present only for monsters originating from MonsterSpawnScr. The
    // Creature stays in Map's pool with hp == 0 while time_until_respawn
    // counts down, then respawns in place with the same instance_id; a
    // respawn_time of 0 leaves that timer unset, which is death for good.
    std::optional<CreatureSpawn> spawn;

    // Alive AI is orchestrated by Map; Creature::Tick owns lifecycle only.
    void Tick(std::chrono::milliseconds delta);
    void Respawn();
    [[nodiscard]] bool IsAlive() const noexcept;
    [[nodiscard]] CreatureAiStateKind AiState() const noexcept;

private:
    friend class Map;
    friend struct Player;

    Creature(CreatureKind creatureKind, std::int64_t monsterId, const MonsterTable& monsters,
             std::optional<CreatureSpawn> creatureSpawn = std::nullopt);

    void Bind(EventBus& events) noexcept;

    // Applies damage and performs the death transition immediately; only
    // Player can initiate this first combat slice.
    std::optional<DamageResult> TakeDamage(std::uint32_t damage, std::uint32_t killerId);

    // Owns the entire death transition, including whether it is terminal
    // (no CreatureSpawn, or a respawn_time of 0) or starts the respawn clock.
    DamageResult Kill(std::uint32_t killerId, std::uint32_t damage);

    void TickRespawn(std::chrono::milliseconds delta);

    static const MonsterRecord& RequireMonsterTemplate(const MonsterTable& monsters,
                                                       std::int64_t monsterId);

    CreatureAi m_ai;

    // Non-owning: every Creature lives in the Map that owns this bus. A raw
    // pointer preserves move assignment required by Pool's dense swap-pop.
    EventBus* m_events = nullptr;
};
