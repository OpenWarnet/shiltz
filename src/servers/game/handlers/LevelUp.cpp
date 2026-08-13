#include "LevelUp.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "parser/LevelScr.h"
#include "protocol/client/LevelUpCheck.h"
#include "protocol/server/LevelUpFail.h"
#include "protocol/server/LevelUpSucc.h"
#include "tables/GameData.h"
#include "tables/LevelTable.h"

#include <iostream>

void HandleLevelUpCheck(const GameContext& ctx, const LevelUpCheck& request)
{
    std::cout << "Level up check: session_id " << request.session_id << "\n";

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout
            << "Rejecting CG_LEVEL_UP_CHECK: socket has no resolved character (never entered)\n";
        return;
    }

    std::int32_t level = session->player.level;
    std::int64_t exp = session->player.exp;
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

    PayloadWriter writer;

    if (leveledUp)
    {
        session->player.level = level;
        session->player.exp = exp;
        session->player.stats.raw.unallocated_stat_points +=
            static_cast<std::uint32_t>(statPointsGained);
        session->player.skills.unallocated_sp += static_cast<std::uint32_t>(spGained);
        ctx.sessions.Set(ctx.clientSocket, *session);

        session->player.SaveLevel(ctx.db);
        session->player.SaveRawStats(ctx.db);
        session->player.SaveSkillPoints(ctx.db);

        LevelUpSucc response{
            .level = level,
            .unallocated_stat_points =
                static_cast<std::int32_t>(session->player.stats.raw.unallocated_stat_points),
            .unallocated_sp = static_cast<std::int32_t>(session->player.skills.unallocated_sp),
            .unallocated_ep = static_cast<std::int32_t>(session->player.skills.unallocated_ep),
            .current_exp = exp,
        };
        std::cout << "Sending GC_LEVEL_UP_SUCC: level " << response.level
                  << ", unallocated_stat_points " << response.unallocated_stat_points
                  << ", unallocated_sp " << response.unallocated_sp << ", current_exp "
                  << response.current_exp << "\n";
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_LEVEL_UP_SUCC, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    }
    else
    {
        LevelUpFail response{
            .level = level,
            .exp = static_cast<std::int32_t>(exp),
        };
        std::cout << "Sending GC_LEVEL_UP_FAIL: level " << response.level << ", exp "
                  << response.exp << "\n";
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_LEVEL_UP_FAIL, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    }
}
