#pragma once

#include "parser/MonsterScr.h" // MonsterRecord

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

// A monster's current AI behavior -- see Map::TickCreature. NPCs (kind ==
// CreatureKind::Npc) never leave CreatureAiState::Idle; they're static
// dialogue/shop/warp entities, not mobs.
enum class CreatureAiState : std::uint8_t
{
    Idle,
    Wander,
    Attacking,
};

// A single spawned creature on a Map -- one instance from either an
// npcNN.scr NpcInstance (kind == Npc) or an mNN.scr MonsterSpawnInstance
// (kind == Monster). `monster_id` joins NpcSpawn::id or MonsterRecord::id
// depending on `kind`. `instance_id` has nothing to do with the .scr data
// itself -- it's assigned by World (see World::AllocateCreatureInstanceId)
// to identify this one spawned instance uniquely across the whole World,
// the way GC_CRT_LOAD's own entity id does on the wire.
struct Creature
{
    std::uint32_t instance_id = 0;
    CreatureKind kind = CreatureKind::Monster;
    std::int64_t monster_id = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t direction = 0;

    // A copy of monster_id's monster.scr row, taken once at spawn time
    // (see MapLoader::Load) rather than looked up from MonsterTable on
    // every access -- Map's simulation code (Map.cpp/AdvanceAiState) has
    // no reference to GameData/MonsterTable at all, only to creatures
    // already carrying what they need. Default-constructed (every field
    // 0) if monster_id wasn't actually in monster.scr.
    MonsterRecord monster;

    // AI state (Monster kind only -- see Map::TickCreature). ai_timer
    // counts down by each World tick's delta; when it reaches zero the
    // creature re-rolls its next state. ai_decision_seq bumps once per
    // roll and feeds the decision's pseudo-random seed alongside
    // instance_id, so no roll -- for any creature, ever -- repeats.
    CreatureAiState ai_state = CreatureAiState::Idle;
    std::chrono::milliseconds ai_timer{0};
    std::uint32_t ai_decision_seq = 0;

    // How many more steps are left in the current wander bout (see
    // Map::AdvanceAiState) -- 0 means either not wandering, or on the last
    // step of one. Set from monster.scr's wander_step_count when a Wander
    // roll starts a fresh bout; each step thereafter decrements it instead
    // of re-rolling Idle vs. Wander, so a monster can cross more than one
    // grid tile per decision instead of re-rolling every single tile.
    std::uint32_t wander_steps_remaining = 0;

    // Index into Map.cpp's kWanderOffsets, rolled once when a wander bout
    // starts and reused for every step of that bout -- keeps a multi-step
    // bout a straight walk in one heading instead of re-rolling a fresh
    // random direction (and zigzagging) every step.
    std::uint8_t wander_direction_index = 0;

    // Player::instance_id of whoever this creature is currently attacking
    // -- only meaningful while ai_state == Attacking (see
    // Map::AttackCreature). Left as-is once set; nothing clears it back to
    // 0 yet, since TickCreature's next Idle/Wander roll overwrites ai_state
    // anyway (see Map.cpp) -- actually acting on Attacking is future work.
    std::uint32_t ai_target_id = 0;
};
