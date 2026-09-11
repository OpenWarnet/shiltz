#include "world/Character.h"

#include "protocol/server/CharacterDataLoad.h"
#include "protocol/server/InventoryItemList.h"
#include "repositories/CharacterRepository.h"
#include "repositories/ItemRepository.h"
#include "repositories/QuestFlagRepository.h"
#include "repositories/SkillRepository.h"
#include "storage/IDatabase.h"

#include <utility>

CharacterDerivedStats operator+(const CharacterDerivedStats& a, const CharacterDerivedStats& b)
{
    CharacterDerivedStats sum;
    sum.max_hp = a.max_hp + b.max_hp;
    sum.max_ap = a.max_ap + b.max_ap;
    sum.damage = a.damage + b.damage;
    sum.defense = a.defense + b.defense;
    sum.magic = a.magic + b.magic;
    sum.accuracy = a.accuracy + b.accuracy;
    sum.evasion = a.evasion + b.evasion;
    sum.critical = a.critical + b.critical;
    sum.attack_speed = a.attack_speed + b.attack_speed;
    sum.movement_speed = a.movement_speed + b.movement_speed;
    sum.damage_dealt_increase_percent = a.damage_dealt_increase_percent + b.damage_dealt_increase_percent;
    sum.damage_taken_decrease_percent = a.damage_taken_decrease_percent + b.damage_taken_decrease_percent;
    sum.hp_percent_bonus = a.hp_percent_bonus + b.hp_percent_bonus;
    sum.ap_percent_bonus = a.ap_percent_bonus + b.ap_percent_bonus;
    return sum;
}

bool Character::LoadFromDB(IDatabase& db, std::int64_t characterId)
{
    std::optional<CharacterRepository::CoreData> core = CharacterRepository::Load(db, characterId);
    if (!core)
        return false;

    id = characterId;

    name = std::move(core->name);
    level = core->level;
    job_id = core->job_id;
    gender = core->gender;
    hairstyle_id = core->hairstyle_id;
    face_id = core->face_id;

    stats.raw = core->raw_stats;

    money = core->money; // "cegel" on the wire, see ToCharacterDataLoad

    map_id = core->map_id;
    x = core->x;
    y = core->y;

    exp = core->exp;
    hp = core->hp;
    ap = core->ap;
    fame = core->fame;

    // TODO: xp has no DB column yet -- placeholder. Note this is distinct
    // from `exp` (current_exp on the wire, backing the level.scr curve) --
    // xp isn't read anywhere else in the codebase yet.

    equipment = ItemRepository::LoadAllEquipment(db, characterId);
    inventory = ItemRepository::LoadAllInventory(db, characterId);

    skills.unallocated_sp = core->unallocated_sp;
    skills.unallocated_ep = core->unallocated_ep;
    skills.skills = SkillRepository::LoadSkillLevels(db, characterId);

    quest_flags = QuestFlagRepository::LoadAll(db, characterId);

    return true;
}

void Character::SavePosition(IDatabase& db) const
{
    CharacterRepository::SavePosition(db, id, map_id, x, y);
}

void Character::SaveVitals(IDatabase& db) const
{
    CharacterRepository::SaveVitals(db, id, hp, ap);
}

void Character::SaveRawStats(IDatabase& db) const
{
    CharacterRepository::SaveRawStats(db, id, stats.raw);
}

void Character::SaveSkillPoints(IDatabase& db) const
{
    SkillRepository::SaveSkillPoints(db, id, skills.unallocated_sp,
                                      skills.unallocated_ep);
}

void Character::SaveSkillLevels(IDatabase& db) const
{
    SkillRepository::SaveSkillLevels(db, id, skills.skills);
}

void Character::SaveLevel(IDatabase& db) const
{
    CharacterRepository::SaveLevel(db, id, level, exp);
}

void Character::SetEquipmentSlot(std::uint32_t slot, const Item& item)
{
    for (CharacterEquipmentItem& entry : equipment)
    {
        if (entry.slot == slot)
        {
            entry.item = item;
            return;
        }
    }

    equipment.push_back(CharacterEquipmentItem{.slot = slot, .item = item});
}

void Character::ClearEquipmentSlot(std::uint32_t slot)
{
    std::erase_if(equipment, [slot](const CharacterEquipmentItem& entry) { return entry.slot == slot; });
}

void Character::SetInventorySlot(std::uint32_t slotIndex, const Item& item)
{
    for (CharacterInventoryItem& entry : inventory)
    {
        if (entry.slot_index == slotIndex)
        {
            entry.item = item;
            return;
        }
    }

    inventory.push_back(CharacterInventoryItem{.slot_index = slotIndex, .item = item});
}

void Character::ClearInventorySlot(std::uint32_t slotIndex)
{
    std::erase_if(inventory,
                   [slotIndex](const CharacterInventoryItem& entry) { return entry.slot_index == slotIndex; });
}

void Character::SetItemSlot(std::uint32_t wireSlotId, const Item& item)
{
    const ItemRepository::SlotRef ref = ItemRepository::ResolveSlotRef(wireSlotId);

    if (ref.kind == ItemRepository::SlotKind::Equipment)
        SetEquipmentSlot(ref.index, item);
    else
        SetInventorySlot(ref.index, item);
}

void Character::ClearItemSlot(std::uint32_t wireSlotId)
{
    const ItemRepository::SlotRef ref = ItemRepository::ResolveSlotRef(wireSlotId);

    if (ref.kind == ItemRepository::SlotKind::Equipment)
        ClearEquipmentSlot(ref.index);
    else
        ClearInventorySlot(ref.index);
}

std::optional<Item> Character::GetEquipmentSlot(std::uint32_t slot) const
{
    for (const CharacterEquipmentItem& entry : equipment)
    {
        if (entry.slot == slot)
            return entry.item;
    }

    return std::nullopt;
}

std::optional<Item> Character::GetInventorySlot(std::uint32_t slotIndex) const
{
    for (const CharacterInventoryItem& entry : inventory)
    {
        if (entry.slot_index == slotIndex)
            return entry.item;
    }

    return std::nullopt;
}

std::optional<Item> Character::GetItemSlot(std::uint32_t wireSlotId) const
{
    const ItemRepository::SlotRef ref = ItemRepository::ResolveSlotRef(wireSlotId);

    return ref.kind == ItemRepository::SlotKind::Equipment ? GetEquipmentSlot(ref.index)
                                                            : GetInventorySlot(ref.index);
}

CharacterDataLoad Character::ToCharacterDataLoad(std::uint32_t epsUserFlag,
                                               std::uint32_t serverTimestamp) const
{
    CharacterDataLoad result;
    result.self_entity_id = instance_id;
    result.eps_user_flag = epsUserFlag;
    result.map_id = map_id;
    result.loc_x = static_cast<std::uint32_t>(x);
    result.loc_y = static_cast<std::uint32_t>(y);
    result.level = static_cast<std::uint32_t>(level);
    result.job_id = job_id;
    result.gender = gender;
    result.current_exp = exp;
    result.cegel = money;
    result.fame = fame;
    result.stats_str = stats.raw.strength;
    result.stats_int = stats.raw.intelligence;
    result.stats_dex = stats.raw.dexterity;
    result.stats_con = stats.raw.constitution;
    result.stats_men = stats.raw.mentality;
    result.stats_sen = stats.raw.sense;
    result.current_hp = hp;
    result.current_ap = ap;
    result.unused_stat_points = stats.raw.unallocated_stat_points;
    result.skill_points = skills.unallocated_sp;
    result.enforced_points = skills.unallocated_ep;
    result.hair_type = hairstyle_id;
    result.quest_flags = quest_flags;
    result.char_name = name;
    result.server_timestamp = serverTimestamp;
    result.face_type = face_id;

    // skill_list is a fixed 64-slot wire array -- silently drop anything
    // past that (no character has come close to 64 learned skills yet, but
    // nothing here enforces it) rather than writing out of bounds.
    for (std::size_t i = 0; i < skills.skills.size() && i < result.skill_list.size(); ++i)
    {
        result.skill_list[i] = CharacterSkillEntry{
            .skill_id = static_cast<std::uint16_t>(skills.skills[i].id),
            .skill_level = static_cast<std::uint16_t>(skills.skills[i].level),
        };
    }

    return result;
}

InventoryItemList Character::ToInventoryItemList() const
{
    InventoryItemList result;
    result.total_count = 0;

    for (const auto& equipped : equipment)
    {
        if (equipped.slot >= InventoryItemList::kBagStartSlot)
            continue;

        result.slots[equipped.slot] = {
            .item_id = equipped.item.item_id,
            .qty_or_refine = equipped.item.WireQuantityOrRefine(),
            .option_bits = equipped.item.option_bits,
        };
    }

    for (const auto& stored : inventory)
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

    return result;
}

