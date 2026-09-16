#pragma once

#include "world/systems/System.h"

class GameData;
class Map;
class Outbox;
struct Player;
struct PlayerAttackResolvedEvent;
struct MonsterKilledEvent;

// Replicates committed combat events to clients. Target resolution,
// validation, damage, death, and EXP ownership stay in Player/Creature.
class CombatSystem : public System
{
public:
    CombatSystem(Map& map, const Outbox& outbox, const GameData& data);

    CombatSystem(const CombatSystem&) = delete;
    CombatSystem& operator=(const CombatSystem&) = delete;

    void SendMonsterKilled(const MonsterKilledEvent& event) const;
    void SendPlayerAttackResult(const PlayerAttackResolvedEvent& event) const;

private:
    Map& m_map;
    const Outbox& m_outbox;
};
