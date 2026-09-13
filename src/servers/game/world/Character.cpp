#include "world/Character.h"

#include "enums/StatId.h"
#include "repositories/ItemRepository.h"

namespace
{
// Returns nullptr for an out-of-range stat_id.
std::uint32_t* ResolveRawStat(CharacterRawStats& raw, std::int32_t statId)
{
    switch (static_cast<StatId>(statId))
    {
    case StatId::Strength:
        return &raw.strength;
    case StatId::Intelligence:
        return &raw.intelligence;
    case StatId::Dexterity:
        return &raw.dexterity;
    case StatId::Constitution:
        return &raw.constitution;
    case StatId::Mentality:
        return &raw.mentality;
    case StatId::Sense:
        return &raw.sense;
    default:
        return nullptr;
    }
}
} // namespace

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

void Character::SetEquipmentSlot(std::uint32_t slot, const Item& item)
{
    for (CharacterEquipmentItem& entry : equipment)
    {
        if (entry.slot == slot)
        {
            entry.item = item;
            stats.dirty = true;
            return;
        }
    }

    equipment.push_back(CharacterEquipmentItem{.slot = slot, .item = item});
    stats.dirty = true;
}

void Character::ClearEquipmentSlot(std::uint32_t slot)
{
    std::erase_if(equipment, [slot](const CharacterEquipmentItem& entry) { return entry.slot == slot; });
    stats.dirty = true;
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

bool Character::SetItemSlot(std::uint32_t wireSlotId, const Item& item)
{
    const ItemRepository::SlotRef ref = ItemRepository::ResolveSlotRef(wireSlotId);

    if (ref.kind == ItemRepository::SlotKind::Equipment)
    {
        SetEquipmentSlot(ref.index, item);
        return true;
    }

    SetInventorySlot(ref.index, item);
    return false;
}

bool Character::ClearItemSlot(std::uint32_t wireSlotId)
{
    const ItemRepository::SlotRef ref = ItemRepository::ResolveSlotRef(wireSlotId);

    if (ref.kind == ItemRepository::SlotKind::Equipment)
    {
        ClearEquipmentSlot(ref.index);
        return true;
    }

    ClearInventorySlot(ref.index);
    return false;
}

std::optional<std::uint32_t> Character::RaiseStat(std::int32_t statId, std::int32_t amount)
{
    std::uint32_t* rawStat = ResolveRawStat(stats.raw, statId);
    if (!rawStat || amount <= 0 ||
        stats.raw.unallocated_stat_points < static_cast<std::uint32_t>(amount))
        return std::nullopt;

    *rawStat += static_cast<std::uint32_t>(amount);
    stats.raw.unallocated_stat_points -= static_cast<std::uint32_t>(amount);
    stats.dirty = true;
    return *rawStat;
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
