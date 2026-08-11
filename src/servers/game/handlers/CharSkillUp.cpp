#include "CharSkillUp.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "parser/SkillScr.h"
#include "protocol/client/CharSkillUpEx.h"
#include "protocol/server/CharSkillUpExFail.h"
#include "protocol/server/CharSkillUpExSucc.h"
#include "world/Player.h"
#include "world/World.h"

#include <iostream>

namespace
{
    // See protocol/server/CharSkillUpExFail.h for the full table.
    enum class SkillUpFailReason : std::int32_t
    {
        DbError = -1,
        TotalSkillCountError = -2,
        SkillIdNotFound = -3,
        PrereqNotLearned = -4,
        AlreadyMaxLevel = -5,
        LevelTooLow = -6,
        NotEnoughSp = -7,
        JobMismatch = -8,
    };

    std::uint32_t CurrentSkillLevel(const PlayerSkills& skills, std::int64_t skillId)
    {
        for (const auto& skill : skills.skills)
        {
            if (skill.id == static_cast<std::uint32_t>(skillId))
                return skill.level;
        }

        return 0;
    }

    void ApplySkillLevelUp(PlayerSkills& skills, const SkillLevelUpEntry& entry)
    {
        for (auto& skill : skills.skills)
        {
            if (skill.id == static_cast<std::uint32_t>(entry.skill_id))
            {
                skill.level += static_cast<std::uint32_t>(entry.num_level_up);
                return;
            }
        }

        skills.skills.push_back(PlayerSkill{
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
    // given skill_id (verified against the real data files), so level 1's
    // row is enough to check those and to confirm the skill_id exists at
    // all; min_level/skill_points are checked per target level, since
    // those genuinely vary level to level.
    Validation ValidateSkillUpRequest(const World& world, const Player& player,
                                       const std::vector<SkillLevelUpEntry>& entries)
    {
        if (entries.empty())
            return Fail(SkillUpFailReason::TotalSkillCountError);

        std::int64_t totalCost = 0;

        for (const auto& entry : entries)
        {
            if (entry.num_level_up <= 0)
                return Fail(SkillUpFailReason::TotalSkillCountError);

            const SkillRecord* baseRecord = world.FindSkillRecord(entry.skill_id, 1);
            if (!baseRecord)
                return Fail(SkillUpFailReason::SkillIdNotFound);

            if (baseRecord->job_type != 0 &&
                baseRecord->job_type != static_cast<std::int64_t>(player.job_id))
                return Fail(SkillUpFailReason::JobMismatch);

            if (baseRecord->prereq_skill_id != 0 &&
                CurrentSkillLevel(player.skills, baseRecord->prereq_skill_id) <
                    static_cast<std::uint32_t>(baseRecord->prereq_skill_level))
                return Fail(SkillUpFailReason::PrereqNotLearned);

            const std::int64_t currentLevel = CurrentSkillLevel(player.skills, entry.skill_id);
            if (currentLevel + entry.num_level_up > baseRecord->max_skill_level)
                return Fail(SkillUpFailReason::AlreadyMaxLevel);

            for (std::int64_t targetLevel = currentLevel + 1;
                 targetLevel <= currentLevel + entry.num_level_up; ++targetLevel)
            {
                const SkillRecord* record = world.FindSkillRecord(entry.skill_id, targetLevel);
                if (!record)
                    return Fail(SkillUpFailReason::SkillIdNotFound);

                if (static_cast<std::int64_t>(player.level) < record->min_level)
                    return Fail(SkillUpFailReason::LevelTooLow);

                totalCost += record->skill_points;
            }
        }

        if (static_cast<std::int64_t>(player.skills.unallocated_sp) < totalCost)
            return Fail(SkillUpFailReason::NotEnoughSp);

        return Validation{.ok = true, .totalCost = totalCost};
    }
}

void HandleCharSkillUpEx(const GameContext& ctx, const CharSkillUpEx& request)
{
    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout
            << "Rejecting CG_CHAR_SKILL_UP_EX: socket has no resolved character (never entered)\n";
        return;
    }

    const Validation validation =
        ValidateSkillUpRequest(ctx.world, session->player, request.skills);

    std::cout << "Char skill up ex: " << request.skills.size() << " skill(s), valid "
              << validation.ok << ", total cost " << validation.totalCost << "\n";

    PayloadWriter writer;

    if (validation.ok)
    {
        PlayerSkills& skills = session->player.skills;

        for (const auto& entry : request.skills)
            ApplySkillLevelUp(skills, entry);

        skills.unallocated_sp -= static_cast<std::uint32_t>(validation.totalCost);
        ctx.sessions.Set(ctx.clientSocket, *session);
        session->player.SaveSkillPoints(ctx.db);
        session->player.SaveSkillLevels(ctx.db);

        CharSkillUpExSucc response{
            .remaining_sp = static_cast<std::int32_t>(skills.unallocated_sp),
            .remaining_ep = static_cast<std::int32_t>(skills.unallocated_ep),
        };
        std::cout << "Sending GC_CHAR_SKILL_UP_EX_SUCC: remaining_sp " << response.remaining_sp
                  << ", remaining_ep " << response.remaining_ep << "\n";
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_CHAR_SKILL_UP_EX_SUCC, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    }
    else
    {
        CharSkillUpExFail response{.reason = static_cast<std::int32_t>(validation.failReason)};
        std::cout << "Sending GC_CHAR_SKILL_UP_EX_FAIL: reason " << response.reason << "\n";
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_CHAR_SKILL_UP_EX_FAIL, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    }
}
