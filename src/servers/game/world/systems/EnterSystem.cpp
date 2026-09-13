#include "EnterSystem.h"

#include "Outbox.h"
#include "protocol/server/CharacterDataLoad.h"
#include "protocol/server/InventoryItemList.h"
#include "world/Map.h"
#include "world/Player.h"
#include "world/events/CharacterEvents.h"

#include <cstdint>
#include <ctime>

EnterSystem::EnterSystem(Map& map, const Outbox& outbox, const GameData& data)
    : m_map(map), m_outbox(outbox), m_data(data)
{
    map.Events().On<CharacterJoinEvent>().Register<&EnterSystem::SendCharacterDataLoad>(*this);
    map.Events().On<CharacterJoinEvent>().Register<&EnterSystem::SendInventoryItemList>(*this);
}

void EnterSystem::SendCharacterDataLoad(const CharacterJoinEvent& event) const
{
    const Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    const Character& character = player->character;

    CharacterDataLoad result;
    result.self_entity_id = character.instance_id;
    result.eps_user_flag = 1; // TODO: no DB column -- kept as the prior hardcoded placeholder
    result.map_id = character.map_id;
    result.loc_x = character.x;
    result.loc_y = character.y;
    result.level = static_cast<std::uint32_t>(character.level);
    result.job_id = character.job_id;
    result.gender = character.gender;
    result.current_exp = character.exp;
    result.cegel = character.money;
    result.fame = character.fame;
    result.stats_str = character.stats.raw.strength;
    result.stats_int = character.stats.raw.intelligence;
    result.stats_dex = character.stats.raw.dexterity;
    result.stats_con = character.stats.raw.constitution;
    result.stats_men = character.stats.raw.mentality;
    result.stats_sen = character.stats.raw.sense;
    result.current_hp = character.hp;
    result.current_ap = character.ap;
    result.unused_stat_points = character.stats.raw.unallocated_stat_points;
    result.skill_points = character.skills.unallocated_sp;
    result.enforced_points = character.skills.unallocated_ep;
    result.hair_type = character.hairstyle_id;
    result.quest_flags = character.quest_flags;
    result.server_timestamp = static_cast<std::uint32_t>(std::time(nullptr));
    result.face_type = character.face_id;

    // skill_list is a fixed 64-slot wire array -- silently drop anything past that (no character
    // has come close to 64 learned skills yet, but nothing here enforces it) rather than writing
    // out of bounds.
    const auto& skills = character.skills.skills;
    for (std::size_t i = 0; i < skills.size() && i < result.skill_list.size(); ++i)
    {
        result.skill_list[i] = CharacterSkillEntry{
            .skill_id = static_cast<std::uint16_t>(skills[i].id),
            .skill_level = static_cast<std::uint16_t>(skills[i].level),
        };
    }

    m_outbox.Send(player->connection, result);
}

void EnterSystem::SendInventoryItemList(const CharacterJoinEvent& event) const
{
    const Player* player = m_map.GetPlayer(event.instance_id);
    if (!player)
        return;

    const Character& character = player->character;

    InventoryItemList result;
    result.total_count = 0;

    for (const auto& equipped : character.equipment)
    {
        if (equipped.slot >= InventoryItemList::kBagStartSlot)
            continue;

        result.slots[equipped.slot] = {
            .item_id = equipped.item.item_id,
            .qty_or_refine = equipped.item.WireQuantityOrRefine(),
            .option_bits = equipped.item.option_bits,
        };
    }

    for (const auto& stored : character.inventory)
    {
        const std::size_t wireSlot = InventoryItemList::kBagStartSlot + stored.slot_index;
        if (wireSlot >= InventoryItemList::kTotalSlots)
            continue;

        result.slots[wireSlot] = {
            .item_id = stored.item.item_id,
            .qty_or_refine = stored.item.WireQuantityOrRefine(),
            .option_bits = stored.item.option_bits,
        };
    }

    m_outbox.Send(player->connection, result);
}
