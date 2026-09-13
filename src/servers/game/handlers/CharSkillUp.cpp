#include "CharSkillUp.h"

#include "Outbox.h"
#include "Persistence.h"
#include "enums/SkillUpFailReason.h"
#include "parser/SkillScr.h"
#include "protocol/client/CharSkillUpEx.h"
#include "protocol/server/CharSkillUpExFail.h"
#include "protocol/server/CharSkillUpExSucc.h"
#include "repositories/SkillRepository.h"
#include "tables/GameData.h"
#include "world/Player.h"
#include "tables/SkillTable.h"

#include <string>

namespace
{
    std::uint32_t CurrentSkillLevel(const CharacterSkills& skills, std::int64_t skillId)
    {
        for (const auto& skill : skills.skills)
        {
            if (skill.id == static_cast<std::uint32_t>(skillId))
                return skill.level;
        }

        return 0;
    }

    void ApplySkillLevelUp(CharacterSkills& skills, const SkillLevelUpEntry& entry)
    {
        for (auto& skill : skills.skills)
        {
            if (skill.id == static_cast<std::uint32_t>(entry.skill_id))
            {
                skill.level += static_cast<std::uint32_t>(entry.num_level_up);
                return;
            }
        }

        skills.skills.push_back(CharacterSkill{
            .id = static_cast<std::uint32_t>(entry.skill_id),
            .level = static_cast<std::uint32_t>(entry.num_level_up),
        });
    }

    struct Validation
    {
        bool ok = true;
        SkillUpFailReason failReason = SkillUpFailReason::TotalSkillCountError;
        std::int64_t totalCost = 0;
    };

    Validation Fail(SkillUpFailReason reason)
    {
        return Validation{.ok = false, .failReason = reason};
    }

    // Walks every requested skill/level-up, checking it against skillNN.scr
    // (job/prereq/max-level/character-level) and tallying its real
    // skill_points cost, before anything is mutated. job_type/prereq_*/
    // max_skill_level are constant across every skillNN.scr row for a
    // given skill_id, so level 1's row is enough to check those and to
    // confirm the skill_id exists at all; min_level/skill_points are
    // checked per target level, since those genuinely vary level to level.
    Validation ValidateSkillUpRequest(const SkillTable& skills, const Character& character,
                                       const std::vector<SkillLevelUpEntry>& entries)
    {
        if (entries.empty())
            return Fail(SkillUpFailReason::TotalSkillCountError);

        std::int64_t totalCost = 0;

        for (const auto& entry : entries)
        {
            if (entry.num_level_up <= 0)
                return Fail(SkillUpFailReason::TotalSkillCountError);

            const SkillRecord* baseRecord = skills.Find(entry.skill_id, 1);
            if (!baseRecord)
                return Fail(SkillUpFailReason::SkillIdNotFound);

            if (baseRecord->job_type != 0 &&
                baseRecord->job_type != static_cast<std::int64_t>(character.job_id))
                return Fail(SkillUpFailReason::JobMismatch);

            if (baseRecord->prereq_skill_id != 0 &&
                CurrentSkillLevel(character.skills, baseRecord->prereq_skill_id) <
                    static_cast<std::uint32_t>(baseRecord->prereq_skill_level))
                return Fail(SkillUpFailReason::PrereqNotLearned);

            const std::int64_t currentLevel = CurrentSkillLevel(character.skills, entry.skill_id);
            if (currentLevel + entry.num_level_up > baseRecord->max_skill_level)
                return Fail(SkillUpFailReason::AlreadyMaxLevel);

            for (std::int64_t targetLevel = currentLevel + 1;
                 targetLevel <= currentLevel + entry.num_level_up; ++targetLevel)
            {
                const SkillRecord* record = skills.Find(entry.skill_id, targetLevel);
                if (!record)
                    return Fail(SkillUpFailReason::SkillIdNotFound);

                if (static_cast<std::int64_t>(character.level) < record->min_level)
                    return Fail(SkillUpFailReason::LevelTooLow);

                totalCost += record->skill_points;
            }
        }

        if (static_cast<std::int64_t>(character.skills.unallocated_sp) < totalCost)
            return Fail(SkillUpFailReason::NotEnoughSp);

        return Validation{.ok = true, .totalCost = totalCost};
    }
}

void HandleCharSkillUpEx(const GameContext& ctx, const CharSkillUpEx& request, Player& player)
{
    Character& character = player.character;
    const Validation validation = ValidateSkillUpRequest(ctx.data.skills, character, request.skills);

    if (!validation.ok)
    {
        CharSkillUpExFail response;
        response.reason = static_cast<std::int32_t>(validation.failReason);
        ctx.outbox.Send(ctx.connection, response);
        return;
    }

    CharacterSkills& skills = character.skills;

    for (const auto& entry : request.skills)
        ApplySkillLevelUp(skills, entry);

    skills.unallocated_sp -= static_cast<std::uint32_t>(validation.totalCost);

    CharSkillUpExSucc response;
    response.remaining_sp = static_cast<std::int32_t>(skills.unallocated_sp);
    response.remaining_ep = static_cast<std::int32_t>(skills.unallocated_ep);
    auto reply = [ctx, response] { ctx.outbox.Send(ctx.connection, response); };

    // Replies either way: a failed save is rewritten by the next save of these fields, or rolls back on relog.
    ctx.persistence.Run(
        [saved = character](IDatabase& db)
        {
            SkillRepository::SaveSkillPoints(db, saved.id, saved.skills.unallocated_sp,
                                              saved.skills.unallocated_ep);
            SkillRepository::SaveSkillLevels(db, saved.id, saved.skills.skills);
        },
        reply, [reply](const std::string&) { reply(); });
}
