#pragma once

#include "parser/MonsterScr.h"
#include "world/Combat.h"
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
    std::uint32_t sequence = 0;

    std::pair<std::uint32_t, std::uint32_t> RollPosition(std::int64_t monsterId,
                                                        std::uint32_t radius,
                                                        std::uint32_t maxCoordinate);

private:
    std::uint64_t MixSpawnSeed(std::int64_t monsterId);
    static std::uint32_t RollCoordinate(std::uint32_t anchor, std::uint32_t radius,
                                        std::uint32_t maxCoordinate, std::uint64_t roll);
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

// A monster's current AI behavior. NPCs (kind ==
// CreatureKind::Npc) never leave CreatureAiState::Idle; they're static
// dialogue/shop/warp entities, not mobs.
enum class CreatureAiState : std::uint8_t
{
    Idle,
    Wander,
};

// Kill performs the whole Alive -> dead transition in one step: Dead is
// terminal (no CreatureSpawn, or a respawn_time of 0), WaitingForRespawn
// carries the CreatureSpawn::time_until_respawn that Tick counts down, and
// RespawnPending means that countdown finished and Map owes this creature a
// Respawn.
enum class CreatureLifecycle : std::uint8_t
{
    Alive,
    Dead,
    WaitingForRespawn,
    RespawnPending,
};

struct CreatureMove
{
    std::uint32_t creature_id = 0;
    std::uint32_t from_x = 0;
    std::uint32_t from_y = 0;
    std::uint32_t to_x = 0;
    std::uint32_t to_y = 0;
    std::uint32_t movement_mode = 1;
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
    // Creature stays in Map's pool with hp <= 0 while this state counts
    // down, then respawns in place with the same instance_id.
    std::optional<CreatureSpawn> spawn;

    CreatureLifecycle lifecycle = CreatureLifecycle::Alive;

    // AI state (Monster kind only). ai_timer
    // counts down by each World tick's delta; when it reaches zero the
    // creature re-rolls its next state. ai_decision_seq bumps once per
    // roll and feeds the decision's pseudo-random seed alongside
    // instance_id, so no roll -- for any creature, ever -- repeats.
    CreatureAiState ai_state = CreatureAiState::Idle;
    std::chrono::milliseconds ai_timer{0};
    std::uint32_t ai_decision_seq = 0;

    void Tick(std::chrono::milliseconds delta, std::uint32_t maxCoordinate);
    void Respawn(std::uint32_t maxCoordinate);
    [[nodiscard]] bool IsAlive() const noexcept;
    [[nodiscard]] bool NeedsRespawn() const noexcept;
    std::optional<CreatureMove> TakePendingMove();

private:
    friend class Map;
    friend struct Player;

    Creature(CreatureKind creatureKind, std::int64_t monsterId, const MonsterTable& monsters,
             std::optional<CreatureSpawn> creatureSpawn = std::nullopt);

    void Bind(EventBus& events) noexcept;

    // Applies damage and performs the death transition immediately. Only
    // Player can initiate this first combat slice; Tick handles the later
    // respawn timer and never discovers death from hp after the fact.
    std::optional<DamageResult> TakeDamage(std::uint32_t damage, std::uint32_t killerId);

    // Owns the entire death transition, respawn state included.
    DamageResult Kill(std::uint32_t killerId, std::uint32_t damage);

    // Decides at the moment of death whether it is terminal, and starts the clock if it isn't.
    void ArmRespawn();

    void TickAi(std::chrono::milliseconds delta, std::uint32_t maxCoordinate);
    void TickRespawn(std::chrono::milliseconds delta);
    void RollNextAiState(std::uint32_t maxCoordinate);
    std::uint64_t MixDecisionSeed();
    static std::uint32_t StepCoordinate(std::uint32_t coordinate, std::int32_t delta,
                                        std::uint32_t maxCoordinate);

    static const MonsterRecord& RequireMonsterTemplate(const MonsterTable& monsters,
                                                       std::int64_t monsterId);

    std::optional<CreatureMove> m_pendingMove;

    // Non-owning: every Creature lives in the Map that owns this bus. A raw
    // pointer preserves move assignment required by Pool's dense swap-pop.
    EventBus* m_events = nullptr;
};
