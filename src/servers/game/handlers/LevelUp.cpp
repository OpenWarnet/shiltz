#include "LevelUp.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "parser/LevelScr.h"
#include "protocol/client/LevelUpCheck.h"
#include "protocol/server/LevelUpFail.h"
#include "protocol/server/LevelUpSucc.h"
#include "tables/GameData.h"
#include "tables/LevelTable.h"

void HandleLevelUpCheck(const GameContext& ctx, const LevelUpCheck& request)
{
    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
        return;

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

    if (!leveledUp)
    {
        PayloadWriter writer;
        LevelUpFail response;
        response.level = level;
        response.exp = static_cast<std::int32_t>(exp);
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_LEVEL_UP_FAIL, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
        return;
    }

    session->player.level = level;
    session->player.exp = exp;
    session->player.stats.raw.unallocated_stat_points += static_cast<std::uint32_t>(statPointsGained);
    session->player.skills.unallocated_sp += static_cast<std::uint32_t>(spGained);
    ctx.sessions.Set(ctx.clientSocket, *session);

    // SaveLevel/SaveRawStats/SaveSkillPoints are blocking SQLite calls --
    // run them on the DB pool instead of the connection's reactor thread.
    // ctx and a copy of the (already-updated) player are captured by value
    // so both stay valid once this handler returns; server.SendTo() is
    // safe to call from any thread -- it queues onto the connection's own
    // strand internally -- so the reply is sent straight from the pool
    // thread once the writes land.
    Player player = session->player;
    boost::asio::post(ctx.dbPool, [ctx, player, level, exp]() {
        player.SaveLevel(ctx.db);
        player.SaveRawStats(ctx.db);
        player.SaveSkillPoints(ctx.db);

        PayloadWriter writer;
        LevelUpSucc response;
        response.level = level;
        response.unallocated_stat_points =
            static_cast<std::int32_t>(player.stats.raw.unallocated_stat_points);
        response.unallocated_sp = static_cast<std::int32_t>(player.skills.unallocated_sp);
        response.unallocated_ep = static_cast<std::int32_t>(player.skills.unallocated_ep);
        response.current_exp = exp;
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_LEVEL_UP_SUCC, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    });
}
