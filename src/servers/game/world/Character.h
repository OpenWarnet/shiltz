#pragma once

#include "Item.h"
#include "protocol/server/CharacterDataLoad.h" // CharacterQuestFlags
#include "world/Placement.h"
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct CharacterRawStats
{
    std::uint32_t unallocated_stat_points = 0;

    std::uint32_t strength = 0;
    std::uint32_t intelligence = 0;
    std::uint32_t dexterity = 0;
    std::uint32_t constitution = 0;
    std::uint32_t mentality = 0;
    std::uint32_t sense = 0;
};

struct CharacterDerivedStats
{
    std::int32_t max_hp = 0;
    std::int32_t max_ap = 0;
    std::int32_t damage = 0;
    std::int32_t defense = 0;
    std::int32_t magic = 0;
    std::int32_t accuracy = 0;
    std::int32_t evasion = 0;
    std::int32_t critical = 0;
    std::int32_t attack_speed = 0;
    std::int32_t movement_speed = 0;

    std::int32_t damage_dealt_increase_percent = 0;
    std::int32_t damage_taken_decrease_percent = 0;

    // HP%/AP% contributions (currently equipment-only). Unlike every other
    // field here, these don't add directly onto max_hp/max_ap -- they scale
    // the combined total as one final multiplicative step after every stat
    // source has been summed via operator+ (see stats/Stats.h's RecalculateDerivedStats).
    std::int32_t hp_percent_bonus = 0;
    std::int32_t ap_percent_bonus = 0;
};

// Sums every field, including hp_percent_bonus/ap_percent_bonus -- percent
// contributions from multiple sources (equipment, later skill buffs) add
// together before being applied once. See CharacterDerivedStats::hp_percent_bonus.
CharacterDerivedStats operator+(const CharacterDerivedStats& a, const CharacterDerivedStats& b);

struct CharacterStats
{
    CharacterRawStats raw;
    CharacterDerivedStats derived;

    // Set at every mutation that affects derived stats, cleared by Map::RecalculateDirtyStats()
    // once per tick (before event dispatch). Deliberately not the TrinityCore-style "call
    // UpdateAllStats() inline at every site" pattern -- that project has had unfixed staleness bugs
    // (TrinityCore#9652, AzerothCore#23179) from exactly the missed-call-site failure mode for
    // years. Scoped to this Character only: if a future feature derives one character's stats from
    // another's (party buffs, pet inheritance), that dependency needs its own propagation, not just
    // this flag.
    bool dirty = true;
};

struct CharacterSkill
{
    std::uint32_t id = 0;
    std::uint32_t level = 0;
};

struct CharacterSkills
{
    std::uint32_t unallocated_sp = 0;
    std::uint32_t unallocated_ep = 0;
    std::vector<CharacterSkill> skills;
};

struct CharacterEquipmentItem
{
    std::uint32_t slot = 0;
    Item item;
};

struct CharacterInventoryItem
{
    std::uint32_t slot_index = 0;
    Item item;
};

struct Character
{
    // `character` table row id -- what CharacterRepository's load/save functions key on.
    std::int64_t id = 0;

    // Runtime world entity id (EntityIdGenerator::Next()), assigned on CG_ENTER.
    // Sent on the wire as this character's entity id; not persisted.
    std::uint32_t instance_id = 0;

    std::string name;
    std::uint32_t job_id = 0;
    std::uint32_t gender = 0;
    std::uint32_t hairstyle_id = 0;
    std::uint32_t face_id = 0;

    std::uint32_t map_id = 0;
    Placement placement;

    std::int64_t money = 0;

    std::uint32_t hp = 1;
    std::uint32_t ap = 1;
    std::uint32_t xp = 1;

    std::int32_t level = 1;
    std::int64_t exp = 1;
    std::uint32_t fame = 0;

    CharacterStats stats;
    CharacterSkills skills;

    std::vector<CharacterEquipmentItem> equipment;
    std::vector<CharacterInventoryItem> inventory;

    // Quest dialog flags -- backs quest.scr's has_flag/set_flag columns
    // (see handlers/Quest.cpp) and is sent to the client as-is on CG_ENTER
    // (see world/systems/EnterSystem.cpp).
    CharacterQuestFlags quest_flags;

    // Keep equipment/inventory in step with the same wire-slot writes made
    // through ItemRepository -- call alongside every ItemRepository::Save*/
    // Clear* so this connection's cached Character never drifts from the DB
    // rows a handler just wrote (see repositories/ItemRepository.h for the
    // write side of the same slots). Equipment and inventory each get a
    // table-relative overload plus a wire-slot overload that resolves which
    // table it belongs to, mirroring ItemRepository's own split.
    void SetEquipmentSlot(std::uint32_t slot, const Item& item);
    void ClearEquipmentSlot(std::uint32_t slot);

    void SetInventorySlot(std::uint32_t slotIndex, const Item& item);
    void ClearInventorySlot(std::uint32_t slotIndex);

    // Returns true if wireSlotId names an equipment slot (as opposed to a bag slot); flags
    // stats.dirty when it does (via SetEquipmentSlot/ClearEquipmentSlot), since equipping/
    // unequipping changes derived stats -- the next Map::Tick recalculates it, no caller action
    // needed.
    bool SetItemSlot(std::uint32_t wireSlotId, const Item& item);
    bool ClearItemSlot(std::uint32_t wireSlotId);

    // Adds `amount` (must be positive) to the raw stat named by statId, deducting
    // unallocated_stat_points by the same amount and flagging stats.dirty. Returns the stat's new
    // value; nullopt (no-op) if statId is unknown, amount isn't positive, or there aren't enough
    // unallocated points.
    std::optional<std::uint32_t> RaiseStat(std::int32_t statId, std::int32_t amount);

    // Reads from this cache rather than the DB -- valid as long as every
    // write above is kept paired with its ItemRepository::Save*/Clear*
    // call, which is the point of the Set*/Clear* methods above.
    std::optional<Item> GetEquipmentSlot(std::uint32_t slot) const;
    std::optional<Item> GetInventorySlot(std::uint32_t slotIndex) const;
    std::optional<Item> GetItemSlot(std::uint32_t wireSlotId) const;
};
