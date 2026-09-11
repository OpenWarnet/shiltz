#include "SkillRepository.h"

#include "storage/IDatabase.h"

namespace SkillRepository
{
std::vector<CharacterSkill> LoadSkillLevels(IDatabase& db, std::int64_t characterId)
{
    std::vector<CharacterSkill> skills;

    auto findSkills =
        db.Prepare("SELECT skill_id, level FROM character_skill WHERE character_id = ?");
    findSkills->Bind(0, characterId);

    while (findSkills->Step())
    {
        skills.push_back(CharacterSkill{
            .id = static_cast<std::uint32_t>(std::get<int64_t>(findSkills->Column(0))),
            .level = static_cast<std::uint32_t>(std::get<int64_t>(findSkills->Column(1))),
        });
    }

    return skills;
}

void SaveSkillLevels(IDatabase& db, std::int64_t characterId, const std::vector<CharacterSkill>& skills)
{
    for (const auto& skill : skills)
    {
        // UPSERT: the target row may not exist yet (character_skill rows
        // aren't pre-seeded per skill).
        auto stmt = db.Prepare(
            "INSERT INTO character_skill (character_id, skill_id, level) VALUES (?, ?, ?) "
            "ON CONFLICT(character_id, skill_id) DO UPDATE SET level = excluded.level");
        stmt->Bind(0, characterId);
        stmt->Bind(1, static_cast<int64_t>(skill.id));
        stmt->Bind(2, static_cast<int64_t>(skill.level));
        stmt->Step();
    }
}

void SaveSkillPoints(IDatabase& db, std::int64_t characterId, std::uint32_t unallocatedSp,
                      std::uint32_t unallocatedEp)
{
    auto updateSkillPoints = db.Prepare(
        "UPDATE character SET unallocated_sp = ?, unallocated_ep = ? WHERE id = ?");
    updateSkillPoints->Bind(0, static_cast<int64_t>(unallocatedSp));
    updateSkillPoints->Bind(1, static_cast<int64_t>(unallocatedEp));
    updateSkillPoints->Bind(2, characterId);
    updateSkillPoints->Step();
}
} // namespace SkillRepository
