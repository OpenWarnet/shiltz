#pragma once

#include "world/Character.h" // CharacterRawStats

#include <cstdint>
#include <optional>
#include <string>

class IDatabase;

// Owns every DB access for the `character` and `character_position` tables --
// a character's identity, raw stats, position, and vitals. Skill levels/
// points live in SkillRepository, equipment/inventory in ItemRepository, and
// quest flags in QuestFlagRepository despite some of those columns also
// living on `character` -- grouped by feature rather than by table, same as
// BankRepository spans bank_accounts/bank_items.
namespace CharacterRepository
{
    struct CoreData
    {
        std::string name;
        std::int32_t level = 1;
        std::uint32_t job_id = 0;
        std::uint32_t gender = 0;
        std::uint32_t hairstyle_id = 0;
        std::uint32_t face_id = 0;

        CharacterRawStats raw_stats;

        std::int64_t money = 0;

        std::uint32_t map_id = 0;
        std::int32_t x = 0;
        std::int32_t y = 0;

        std::int64_t exp = 1;
        std::uint32_t hp = 1;
        std::uint32_t ap = 1;
        std::uint32_t fame = 0;

        // Read here alongside everything else for load efficiency, but
        // written through SkillRepository::SaveSkillPoints -- see
        // SkillRepository.h.
        std::uint32_t unallocated_sp = 0;
        std::uint32_t unallocated_ep = 0;
    };

    // Single joined SELECT across `character` and `character_position`, for
    // Character::LoadFromDB. std::nullopt if no such character exists.
    std::optional<CoreData> Load(IDatabase& db, std::int64_t characterId);

    // Every Save*/Add*/TrySpend* below is a narrow, single-concern update
    // rather than one big rewrite, so a handler that only changed money
    // doesn't also clobber position/stats with stale in-memory values.

    void SavePosition(IDatabase& db, std::int64_t characterId, std::uint32_t mapId, std::int32_t x,
                       std::int32_t y);

    // Money/fame/exp are relative (money = money +/- ?) rather than an
    // absolute SET computed from a possibly-stale in-memory read, so two
    // pipelined writes for the same character (see GameSessionStore.h)
    // compose correctly instead of one silently clobbering the other.
    //
    // TrySpendMoney only applies if the row's *current* money >= amount,
    // checked atomically in the same statement -- returns the resulting
    // money on success, nullopt (no write applied) if insufficient. Callers
    // must treat nullopt as a real-time rejection, not assume the debit
    // happened.
    std::optional<std::int64_t> TrySpendMoney(IDatabase& db, std::int64_t characterId,
                                               std::int64_t amount);
    // Pure credit -- no floor check needed. Returns the resulting money.
    std::int64_t AddMoney(IDatabase& db, std::int64_t characterId, std::int64_t amount);

    std::uint32_t AddFame(IDatabase& db, std::int64_t characterId, std::uint32_t amount);
    std::int64_t AddExp(IDatabase& db, std::int64_t characterId, std::int64_t amount);

    void SaveVitals(IDatabase& db, std::int64_t characterId, std::uint32_t hp, std::uint32_t ap);
    void SaveRawStats(IDatabase& db, std::int64_t characterId, const CharacterRawStats& raw);
    void SaveLevel(IDatabase& db, std::int64_t characterId, std::int32_t level, std::int64_t exp);
} // namespace CharacterRepository
