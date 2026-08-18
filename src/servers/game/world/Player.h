#pragma once

#include "Item.h"
#include "protocol/server/CharacterDataLoad.h" // CharacterQuestFlags

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class IDatabase;
struct InventoryItemList;

struct PlayerRawStats
{
    std::uint32_t unallocated_stat_points = 0;

    std::uint32_t strength = 0;
    std::uint32_t intelligence = 0;
    std::uint32_t dexterity = 0;
    std::uint32_t constitution = 0;
    std::uint32_t mentality = 0;
    std::uint32_t sense = 0;
};

struct PlayerDerivedStats
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
    // source has been summed via operator+ (see RecalculateDerivedStats in
    // Stats.cpp).
    std::int32_t hp_percent_bonus = 0;
    std::int32_t ap_percent_bonus = 0;
};

// Sums every field, including hp_percent_bonus/ap_percent_bonus -- percent
// contributions from multiple sources (equipment, later skill buffs) add
// together before being applied once. See PlayerDerivedStats::hp_percent_bonus.
PlayerDerivedStats operator+(const PlayerDerivedStats& a, const PlayerDerivedStats& b);

struct PlayerStats
{
    PlayerRawStats raw;
    PlayerDerivedStats derived;
};

struct PlayerSkill
{
    std::uint32_t id = 0;
    std::uint32_t level = 0;
};

struct PlayerSkills
{
    std::uint32_t unallocated_sp = 0;
    std::uint32_t unallocated_ep = 0;
    std::vector<PlayerSkill> skills;
};

struct PlayerEquipmentItem
{
    std::uint32_t slot = 0;
    Item item;
};

struct PlayerInventoryItem
{
    std::uint32_t slot_index = 0;
    Item item;
};

struct Player
{
    std::uint32_t instance_id = 0;

    std::string name;
    std::uint32_t job_id = 0;
    std::uint32_t gender = 0;
    std::uint32_t hairstyle_id = 0;
    std::uint32_t face_id = 0;

    std::uint32_t map_id = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t direction = 0;

    std::vector<std::pair<std::int32_t, std::int32_t>> known_zones;

    std::int64_t money = 0;

    std::uint32_t hp = 1;
    std::uint32_t ap = 1;
    std::uint32_t xp = 1;

    std::int32_t level = 1;
    std::int64_t exp = 1;
    std::uint32_t fame = 0;

    PlayerStats stats;
    PlayerSkills skills;

    std::vector<PlayerEquipmentItem> equipment;
    std::vector<PlayerInventoryItem> inventory;

    // Quest dialog flags -- backs quest.scr's has_flag/set_flag columns
    // (see handlers/Quest.cpp) and is sent to the client as-is on CG_ENTER
    // (see ToCharacterDataLoad below).
    CharacterQuestFlags quest_flags;

    // Populates this Player from characterId's DB rows. False if no such
    // character exists.
    bool LoadFromDB(IDatabase& db, std::int64_t characterId);

    // Persists `map_id`/`x`/`y` alone.
    void SavePosition(IDatabase& db) const;

    // money/fame have no Player-level Save* wrapper -- every caller now
    // goes through CharacterRepository::TrySpendMoney/AddMoney/AddFame
    // directly (relative, guarded writes; see CharacterRepository.h) and
    // mirrors the DB's confirmed result into `money`/`fame` itself, rather
    // than flushing a value already mutated in-place here.

    // Persists `hp` and `ap` together -- both are touched together by quest
    // rewards (see handlers/Quest.cpp), so one narrow update covers both
    // without also rewriting position/stats.
    void SaveVitals(IDatabase& db) const;

    // Persists the six named raw stats (stats.raw.strength..sense) plus
    // stats.raw.unallocated_stat_points.
    void SaveRawStats(IDatabase& db) const;

    // Persists skills.unallocated_sp/unallocated_ep. Kept separate from
    // SaveRawStats since they're a different concern (skill points, not
    // raw stats) and are touched by different handlers.
    void SaveSkillPoints(IDatabase& db) const;

    // Upserts every entry currently in skills.skills into `character_skill`
    // (one row per skill_id). Persisting the whole list rather than a
    // single changed skill mirrors how a CG_CHAR_SKILL_UP_EX request can
    // raise several skills at once -- re-upserting an unchanged skill is
    // harmless (see handlers/CharSkillUp.cpp).
    void SaveSkillLevels(IDatabase& db) const;

    // Persists `level` and `exp` together -- always move in lockstep after
    // a CG_LEVEL_UP_CHECK (see handlers/LevelUp.cpp), so one narrow update
    // covers both without also rewriting position/stats.
    void SaveLevel(IDatabase& db) const;

    // Keep equipment/inventory in step with the same wire-slot writes made
    // through ItemRepository -- call alongside every ItemRepository::Save*/
    // Clear* so this connection's cached Player never drifts from the DB
    // rows a handler just wrote (see repositories/ItemRepository.h for the
    // write side of the same slots). Equipment and inventory each get a
    // table-relative overload plus a wire-slot overload that resolves which
    // table it belongs to, mirroring ItemRepository's own split.
    void SetEquipmentSlot(std::uint32_t slot, const Item& item);
    void ClearEquipmentSlot(std::uint32_t slot);

    void SetInventorySlot(std::uint32_t slotIndex, const Item& item);
    void ClearInventorySlot(std::uint32_t slotIndex);

    void SetItemSlot(std::uint32_t wireSlotId, const Item& item);
    void ClearItemSlot(std::uint32_t wireSlotId);

    // Reads from this cache rather than the DB -- valid as long as every
    // write above is kept paired with its ItemRepository::Save*/Clear*
    // call, which is the point of the Set*/Clear* methods above.
    std::optional<Item> GetEquipmentSlot(std::uint32_t slot) const;
    std::optional<Item> GetInventorySlot(std::uint32_t slotIndex) const;
    std::optional<Item> GetItemSlot(std::uint32_t wireSlotId) const;

    CharacterDataLoad ToCharacterDataLoad(std::uint32_t epsUserFlag,
                                           std::uint32_t serverTimestamp) const;
    InventoryItemList ToInventoryItemList() const;
};
