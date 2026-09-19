#pragma once

#include "CreatureAiTypes.h"
#include "parser/MonsterScr.h"
#include "states/StateSupport.h"
#include "world/Player.h"

#include <cstdint>
#include <optional>
#include <ranges>

namespace creature_ai
{
// Nearest living player within aggro range, lower instance id winning a tie.
template <std::ranges::input_range Players>
std::optional<CreatureAiObservation> SelectAggroCandidate(const Placement& self,
                                                          const MonsterRecord& monster,
                                                          Players&& players)
{
    const std::uint32_t aggroRange = NormalizeMonsterValue(monster.aggro_range);
    if (aggroRange == 0)
        return std::nullopt;

    std::optional<CreatureAiObservation> nearest;
    std::uint32_t nearestDistance = aggroRange + 1;
    for (const Player& player : players)
    {
        const Character& character = player.character;
        if (character.hp == 0)
            continue;

        const std::uint32_t distance = self.DistanceTo(character.placement);
        if (distance > aggroRange)
            continue;

        const bool closer = distance < nearestDistance;
        const bool tieWithLowerId = distance == nearestDistance &&
                                    (!nearest || character.instance_id < nearest->player_id);
        if (closer || tieWithLowerId)
        {
            nearest = CreatureAiObservation{character.instance_id, character.placement};
            nearestDistance = distance;
        }
    }

    return nearest;
}
} // namespace creature_ai
