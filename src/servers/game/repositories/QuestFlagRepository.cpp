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
} // namespace QuestFlagRepository
