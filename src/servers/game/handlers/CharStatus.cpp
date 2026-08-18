#include "CharStatus.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "enums/StatId.h"
#include "protocol/client/CharStatusUp.h"
#include "protocol/server/CharStatusUpFail.h"
#include "protocol/server/CharStatusUpSucc.h"
#include "tables/GameData.h"
#include "world/Player.h"
#include "stats/Stats.h"

namespace
{
    // Returns nullptr for an out-of-range stat_id.
    std::uint32_t* ResolveRawStat(PlayerRawStats& raw, std::int32_t statId)
    {
        switch (static_cast<StatId>(statId))
        {
        case StatId::Strength:
            return &raw.strength;
        case StatId::Intelligence:
            return &raw.intelligence;
        case StatId::Dexterity:
            return &raw.dexterity;
        case StatId::Constitution:
            return &raw.constitution;
        case StatId::Mentality:
            return &raw.mentality;
        case StatId::Sense:
            return &raw.sense;
        default:
            return nullptr;
        }
    }
}

void HandleCharStatusUp(const GameContext& ctx, const CharStatusUp& request)
{
    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
        return;

    PlayerRawStats& raw = session->player.stats.raw;
    std::uint32_t* rawStat = ResolveRawStat(raw, request.stat_id);
    const bool canAfford =
        rawStat && request.amount > 0 &&
        raw.unallocated_stat_points >= static_cast<std::uint32_t>(request.amount);

    if (!canAfford)
    {
        PayloadWriter writer;
        CharStatusUpFail response{
            .unallocated_point_remaining = static_cast<std::int32_t>(raw.unallocated_stat_points),
        };
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_CHAR_STATUS_UP_FAIL, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
        return;
    }

    *rawStat += static_cast<std::uint32_t>(request.amount);
    raw.unallocated_stat_points -= static_cast<std::uint32_t>(request.amount);
    RecalculateDerivedStats(session->player, ctx.data.items, ctx.data.setOptions, ctx.data.statusRates);
    ctx.sessions.Set(ctx.clientSocket, *session);

    // SaveRawStats is a blocking SQLite call -- run it on the DB pool
    // instead of the connection's reactor thread. player is copied by
    // value so it stays valid once this handler returns; server.SendTo()
    // is safe to call from any thread.
    Player player = session->player;
    const std::int32_t statId = request.stat_id;
    const std::int32_t currentStatPoint = static_cast<std::int32_t>(*rawStat);
    const std::int32_t remaining = static_cast<std::int32_t>(raw.unallocated_stat_points);
    boost::asio::post(ctx.dbPool, [ctx, player, statId, currentStatPoint, remaining]() {
        player.SaveRawStats(ctx.db);

        PayloadWriter writer;
        CharStatusUpSucc response{
            .stat_id = statId,
            .current_stat_point = currentStatPoint,
            .unallocated_point_remaining = remaining,
        };
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_CHAR_STATUS_UP_SUCC, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    });
}
