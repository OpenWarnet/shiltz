#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class IDatabase;
struct CharacterDataLoad;
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
    std::uint32_t max_hp = 0;
    std::uint32_t max_ap = 0;
    std::uint32_t damage = 0;
    std::uint32_t defense = 0;
    std::uint32_t magic = 0;
    std::uint32_t accuracy = 0;
    std::uint32_t evasion = 0;
    std::uint32_t critical = 0;
    std::uint32_t attack_speed = 0;

    // Signed, unlike every other field here -- confirmed via live client
    // cross-check (world/Stats.cpp) that this is a per-class flat offset
    // that goes negative for some classes (e.g. Knight, Mage).
    std::int32_t movement_speed = 0;
    std::uint32_t damage_increase = 0;
    std::uint32_t damage_decrease = 0;
};

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

// Mirrors an `equipment_slot` DB row. Not world/Item.h's Item, which is a
// ground-dropped instance.
struct PlayerEquipmentItem
{
    std::uint32_t slot = 0;
    std::uint32_t item_id = 0;
    std::uint32_t refine_level = 0;
};

// Mirrors an `inventory_slot` DB row. has_refine_level tracks the column's
// NULL-ness: set means an equippable item (quantity meaningless), unset
// means a stackable item (quantity set).
struct PlayerInventoryItem
{
    std::uint32_t slot_index = 0;
    std::uint32_t item_id = 0;
    std::uint32_t quantity = 0;
    std::uint32_t refine_level = 0;
    bool has_refine_level = false;
};

// Content of a single equipment-or-inventory wire slot, as exchanged by
// LoadItemSlot/SaveItemSlot. Same has_refine_level discriminant as
// PlayerInventoryItem.
//
// item_level/item_opt2/option_bits/option_eligible_mask back the NPC magic-
// option appraiser (see handlers/ItemConfirmNpc.h) -- nothing in this
// codebase generates real per-instance values for these yet, so they
// default to a permissive placeholder (item_opt2 = -1 exempts the item
// from the appraiser's level gate; option_eligible_mask = all 10 bits set
// makes every option eligible to roll) rather than to values that would
// make the appraiser a no-op on every item. See migration
// 0010_add_item_magic_option_columns.sql.
struct PlayerItemSlot
{
    std::uint32_t item_id = 0;
    std::uint32_t quantity = 0;
    std::uint32_t refine_level = 0;
    bool has_refine_level = false;

    std::int32_t item_level = 0;
    std::int32_t item_opt2 = -1;
    std::uint32_t option_bits = 0;
    std::uint32_t option_eligible_mask = 0x3FF;
};

// A connected client's full character data, shared by every game-server
// handler via GameSession -- the source of truth between the DB
// (LoadFromDB/SaveToDB) and the wire protocol (ToCharacterDataLoad/
// ToInventoryItemList).
//
// xp has no DB column yet (see LoadFromDB, which leaves it at its default);
// money/exp/hp/ap/fame are persisted. instance_id/direction/known_zones are
// runtime-only, populated by the handler after LoadFromDB.
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

    // Populates this Player from characterId's DB rows. False if no such
    // character exists.
    bool LoadFromDB(IDatabase& db, std::int64_t characterId);

    void SaveToDB(IDatabase& db) const; // position only -- see LoadFromDB.

    // Persists `money` alone, so callers that only changed money (e.g.
    // Trade.cpp) don't also rewrite position.
    void SaveMoney(IDatabase& db) const;

    // Persists `fame` alone.
    void SaveFame(IDatabase& db) const;

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

    CharacterDataLoad ToCharacterDataLoad(std::uint32_t epsUserFlag,
                                           std::uint32_t serverTimestamp) const;
    InventoryItemList ToInventoryItemList() const;

    // Wire slots 0-12 are equipment, 13+ are inventory (kBagStartSlot) --
    // each lives in its own DB table. These resolve that split so callers
    // never branch on which table a wire slot belongs to.
    std::optional<PlayerItemSlot> LoadItemSlot(IDatabase& db, std::uint32_t wireSlotId) const;
    void SaveItemSlot(IDatabase& db, std::uint32_t wireSlotId, const PlayerItemSlot& content) const;
    void ClearItemSlot(IDatabase& db, std::uint32_t wireSlotId) const;
};
