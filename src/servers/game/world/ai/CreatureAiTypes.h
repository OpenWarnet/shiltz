#pragma once

#include "world/Placement.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <variant>

enum class CreatureAiStateKind : std::uint8_t
{
    Idle,
    Wander,
    Chase,
    Attacking,
};

struct CreatureAiObservation
{
    std::uint32_t player_id = 0;
    Placement placement;
};

struct CreatureAiPerception
{
    // Populated only for an Idle/Wander decision boundary.
    std::optional<CreatureAiObservation> aggro_candidate;

    // Populated only while Chase/Attacking owns a target.
    std::optional<CreatureAiObservation> current_target;
};

// Per-creature AI data shared by every state; entering a state resets it.
struct CreatureAiData
{
    std::chrono::milliseconds timer{0};
    std::uint32_t target_id = 0;
    std::uint32_t steps_remaining = 0;
};

enum class CreatureAiSenseKind : std::uint8_t
{
    None,
    AggroCandidate,
    CurrentTarget,
};

struct CreatureAiSenseRequest
{
    CreatureAiSenseKind kind = CreatureAiSenseKind::None;
    std::uint32_t target_id = 0;
};

struct CreatureWanderIntent
{
    std::int32_t dx = 0;
    std::int32_t dy = 0;
};

struct CreatureChaseIntent
{
    std::uint32_t target_id = 0;
    Placement target_placement;
};

struct CreatureAttackIntent
{
    std::uint32_t target_id = 0;
};

using CreatureAiIntent =
    std::variant<CreatureWanderIntent, CreatureChaseIntent, CreatureAttackIntent>;
