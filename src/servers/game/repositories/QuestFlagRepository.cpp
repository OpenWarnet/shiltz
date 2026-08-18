#include "QuestFlagRepository.h"

#include "storage/IDatabase.h"

namespace QuestFlagRepository
{
CharacterQuestFlags LoadAll(IDatabase& db, std::int64_t characterId)
{
    CharacterQuestFlags flags;

    auto stmt = db.Prepare("SELECT quest_id FROM quest_flags WHERE character_id = ? AND flag = 1");
    stmt->Bind(0, characterId);

    while (stmt->Step())
    {
        const auto flagId = std::get<int64_t>(stmt->Column(0));
        flags.Set(static_cast<std::uint32_t>(flagId), true);
    }

    return flags;
}

void SetFlag(IDatabase& db, std::int64_t characterId, std::int64_t flagId)
{
    // UPSERT: the row may already exist (re-reaching a set_flag consequence
    // is a harmless no-op) or may not exist yet.
    auto stmt = db.Prepare(
        "INSERT INTO quest_flags (character_id, quest_id, flag) VALUES (?, ?, 1) "
        "ON CONFLICT(character_id, quest_id) DO UPDATE SET flag = excluded.flag");
    stmt->Bind(0, characterId);
    stmt->Bind(1, flagId);
    stmt->Step();
}

bool TryClaimFlag(IDatabase& db, std::int64_t characterId, std::int64_t flagId)
{
    // Same upsert shape as SetFlag, but the DO UPDATE only fires (and only
    // then does RETURNING produce a row) if the existing row's flag isn't
    // already 1 -- see ItemRepository.h's compare-and-swap note for the
    // same guarded-upsert idiom. A virgin (character_id, quest_id) pair has
    // no conflicting row, so the INSERT branch always claims it regardless
    // of the guard.
    auto stmt = db.Prepare(
        "INSERT INTO quest_flags (character_id, quest_id, flag) VALUES (?, ?, 1) "
        "ON CONFLICT(character_id, quest_id) DO UPDATE SET flag = excluded.flag "
        "WHERE quest_flags.flag IS NOT 1 "
        "RETURNING flag");
    stmt->Bind(0, characterId);
    stmt->Bind(1, flagId);
    return stmt->Step();
}
} // namespace QuestFlagRepository
