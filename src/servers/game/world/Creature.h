#pragma once

#include <chrono>
#include <cstdint>

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

// A monster's current AI behavior -- see Zone::Tick. NPCs (kind ==
// CreatureKind::Npc) never leave CreatureAiState::Idle; they're static
// dialogue/shop/warp entities, not mobs.
enum class CreatureAiState : std::uint8_t
{
    Idle,
    Wander,
};

// A single spawned creature on a Map -- one instance from either an
// npcNN.scr NpcInstance (kind == Npc) or an mNN.scr MonsterSpawnInstance
// (kind == Monster). `monster_id` joins NpcSpawn::id or MonsterRecord::id
// depending on `kind` -- this struct only carries placement, not the
// template's own stats/behavior data. `instance_id` has nothing to do with
// the .scr data itself -- it's assigned at spawn from EntityIdGenerator::Next()
// (world/common/EntityIdGenerator.h) to identify this one spawned instance
// uniquely across the whole World, the way GC_CRT_LOAD's own entity id
// does on the wire.
struct Creature
{
    std::uint32_t instance_id = 0;
    CreatureKind kind = CreatureKind::Monster;
    std::int64_t monster_id = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t direction = 0;

    // AI state (Monster kind only -- see Zone::Tick). ai_timer
    // counts down by each World tick's delta; when it reaches zero the
    // creature re-rolls its next state. ai_decision_seq bumps once per
    // roll and feeds the decision's pseudo-random seed alongside
    // instance_id, so no roll -- for any creature, ever -- repeats.
    CreatureAiState ai_state = CreatureAiState::Idle;
    std::chrono::milliseconds ai_timer{0};
    std::uint32_t ai_decision_seq = 0;
};
