#include "LevelUp.h"

#include "Outbox.h"
#include "Persistence.h"
#include "parser/LevelScr.h"
#include "protocol/client/LevelUpCheck.h"
#include "protocol/server/LevelUpFail.h"
#include "protocol/server/LevelUpSucc.h"
#include "tables/GameData.h"
#include "tables/LevelTable.h"
#include "world/Player.h"

#include <string>

void HandleLevelUpCheck(const GameContext& ctx, const LevelUpCheck&, Player& player)
{
    Character& character = player.character;
    std::int32_t level = character.level;
    std::int64_t exp = character.exp;
    std::int64_t statPointsGained = 0;
    std::int64_t spGained = 0;
    bool leveledUp = false;

    // level.scr's exp column is the amount level `level` needs to exceed to
    // advance to `level + 1` (not a cumulative total), so a single check
    // can cross several levels at once -- keep consuming thresholds off exp
    // until the next one can't be afforded or the table runs out (max
    // level). Each level crossed also awards that row's stat_points_gained/
    // sp_gained -- ep is deliberately not accumulated here, see LevelScr.h.
    while (const LevelRecord* record = ctx.data.levels.Find(level))
    {
        if (exp <= record->exp)
            break;

        exp -= record->exp;
        ++level;
        statPointsGained += record->stat_points_gained;
        spGained += record->sp_gained;
        leveledUp = true;
    }

    if (!leveledUp)
    {
        LevelUpFail response;
        response.level = level;
        response.exp = static_cast<std::int32_t>(exp);
        ctx.outbox.Send(ctx.connection, response);
        return;
    }

    character.level = level;
    character.exp = exp;
    character.stats.raw.unallocated_stat_points += static_cast<std::uint32_t>(statPointsGained);
    character.skills.unallocated_sp += static_cast<std::uint32_t>(spGained);

    LevelUpSucc response;
    response.level = level;
    response.unallocated_stat_points = static_cast<std::int32_t>(character.stats.raw.unallocated_stat_points);
    response.unallocated_sp = static_cast<std::int32_t>(character.skills.unallocated_sp);
    response.unallocated_ep = static_cast<std::int32_t>(character.skills.unallocated_ep);
    response.current_exp = exp;
    auto reply = [ctx, response] { ctx.outbox.Send(ctx.connection, response); };

    // Replies either way: a failed save is rewritten by the next save of these fields, or rolls back on relog.
    ctx.persistence.Run(
        [saved = character](IDatabase& db)
        {
            saved.SaveLevel(db);
            saved.SaveRawStats(db);
            saved.SaveSkillPoints(db);
        },
        reply, [reply](const std::string&) { reply(); });
}
