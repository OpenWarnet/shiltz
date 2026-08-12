#pragma once

#include "Map.h"
#include "enums/JobId.h"
#include "parser/ItemScr.h"
#include "parser/LevelScr.h"
#include "parser/MonsterScr.h"
#include "parser/SellerScr.h"
#include "parser/SkillScr.h"
#include "parser/StatusScr.h"

#include <cstdint>
#include <unordered_map>

// Owns the game server's simulation state -- for now just a single Map,
// plus the monster.scr template table every Creature::monster_id joins
// against. Lifetime is tied to the GameServer process itself
// (Start()/Shutdown() called once each, from GameServer's
// constructor/destructor), NOT to any individual connection or login
// session -- contrast with GameSessionStore, which is per-connection state
// that comes and goes as clients connect and disconnect. The world
// persists across all of that.
class World
{
public:
    void Start();
    void Shutdown();

    Map& GetMap();
    const Map& GetMap() const;

    // Hands out the next Creature::instance_id, unique across every
    // creature spawned in this World (not per-map -- there's only one
    // World). Starts at 10000, a number clear of anything the .scr data
    // itself uses, and just increments; good enough until creatures can
    // despawn and ids need to be reclaimed.
    std::uint32_t AllocateCreatureInstanceId();

    // Looks up a monster.scr row by MonsterRecord::id -- the same
    // id-space as Creature::monster_id. Returns nullptr if this World
    // hasn't loaded that id (e.g. a spawn file referencing an id that
    // isn't actually in monster.scr).
    const MonsterRecord* FindMonsterRecord(std::int64_t monsterId) const;

    // Looks up a seller.scr row by SellerRecord::shop_id -- the same
    // id-space as MonsterRecord::seller_id. Returns nullptr if this World
    // hasn't loaded that shop id.
    const SellerRecord* FindSellerRecord(std::int64_t shopId) const;

    // Looks up an itemNN.scr row by ItemRecord::id -- the same id-space as
    // inventory_slot.item_id. Returns nullptr if this World hasn't loaded
    // that item id.
    const ItemRecord* FindItemRecord(std::int64_t itemId) const;

    // Looks up a level.scr row by LevelRecord::level -- the same id-space
    // as Player::level. Returns nullptr if this World hasn't loaded that
    // level (e.g. it's past the max level in the table).
    const LevelRecord* FindLevelRecord(std::int64_t level) const;

    // Looks up a skillNN.scr row by (SkillRecord::id, level) -- level comes
    // from which skillNN.scr file the row was loaded from (see
    // World::Start), the same id-space as PlayerSkill::level. Returns
    // nullptr if this World hasn't loaded that skill/level combination.
    const SkillRecord* FindSkillRecord(std::int64_t skillId, std::int64_t level) const;

    // status.scr blockIds. Blocks 7, 9, and 10 have no confirmed reader;
    // block 11 (kClownDamageBonusBlock) is genuinely sparse -- present
    // only for JobId::Clown -- rather than "loaded but unused".
    static constexpr std::size_t kDamageBlock = 0;
    static constexpr std::size_t kMagicBlock = 1;
    static constexpr std::size_t kDefenseBlock = 2;
    static constexpr std::size_t kAccuracyBlock = 3;
    static constexpr std::size_t kCriticalBlock = 4;
    static constexpr std::size_t kEvasionBlock = 5;
    static constexpr std::size_t kMaxHpBlock = 6;
    static constexpr std::size_t kApBlock = 8;
    static constexpr std::size_t kClownDamageBonusBlock = 11;

    // Looks up a status.scr rate by (block, jobId). Returns nullptr if
    // this World hasn't loaded that block/job combination -- always
    // true for kClownDamageBonusBlock except at JobId::Clown, since
    // that row simply doesn't exist for anyone else.
    const double* FindStatusRate(std::size_t block, JobId jobId) const;

private:
    Map m_map;
    std::uint32_t m_nextCreatureInstanceId = 10000;
    std::unordered_map<std::int64_t, MonsterRecord> m_monsterRecords;
    std::unordered_map<std::int64_t, SellerRecord> m_sellerRecords;
    std::unordered_map<std::int64_t, ItemRecord> m_itemRecords;
    std::unordered_map<std::int64_t, LevelRecord> m_levelRecords;
    // Keyed by (skillId << 32) | level -- see MakeSkillLevelKey in World.cpp.
    std::unordered_map<std::int64_t, SkillRecord> m_skillRecords;
    // Keyed by (classId << 32) | blockId -- see MakeStatusKey in World.cpp.
    std::unordered_map<std::int64_t, double> m_statusRates;
};
