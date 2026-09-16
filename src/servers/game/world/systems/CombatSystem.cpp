#include "CombatSystem.h"

#include "Outbox.h"
#include "protocol/server/AttackPlayerToCreatureSuccess.h"
#include "protocol/server/AttackToCreatureKill.h"
#include "protocol/server/AttackToCreatureSuccess.h"
#include "protocol/server/CrtKillInfo.h"
#include "world/Creature.h"
#include "world/Map.h"
#include "world/Player.h"
#include "world/Zone.h"
#include "world/events/CombatEvents.h"

#include <cstdint>

CombatSystem::CombatSystem(Map& map, const Outbox& outbox, const GameData&)
    : m_map(map), m_outbox(outbox)
{
    map.Events().On<MonsterKilledEvent>().Register<&CombatSystem::SendMonsterKilled>(*this);
    map.Events().On<PlayerAttackResolvedEvent>().Register<&CombatSystem::SendPlayerAttackResult>(
        *this);
}

void CombatSystem::SendMonsterKilled(const MonsterKilledEvent& event) const
{
    CrtKillInfo info;
    info.exp_gain = event.exp_reward;
    m_outbox.Send(m_map.ViewersOf(Zone::Of(event.x, event.y)), info);
}

void CombatSystem::SendPlayerAttackResult(const PlayerAttackResolvedEvent& event) const
{
    if (const Player* attacker = m_map.GetPlayer(event.attacker_id))
    {
        if (event.outcome == AttackOutcome::Killed)
        {
            AttackToCreatureKill result;
            result.target_instance_id = event.target_id;
            result.target_damage = event.damage;
            result.target_hp = event.target_hp;
            result.exp_gain = event.exp_gain;
            result.display_damage = event.damage;
            m_outbox.Send(attacker->connection, result);
        }
        else
        {
            AttackToCreatureSuccess result;
            result.target_instance_id = event.target_id;
            result.damage = event.damage;
            result.target_hp = static_cast<std::uint32_t>(event.target_hp);
            m_outbox.Send(attacker->connection, result);
        }
    }

    AttackPlayerToCreatureSuccess presented;
    presented.attacker_instance_id = event.attacker_id;
    presented.direction = event.direction;
    presented.player_x = event.origin_x;
    presented.player_y = event.origin_y;
    presented.target_instance_id = event.target_id;
    presented.damage = event.damage;
    presented.target_hp = static_cast<std::uint32_t>(event.target_hp);

    m_outbox.Send(m_map.ViewersOf(Zone::Of(event.target_x, event.target_y)), presented);
}
