#include "CharStatus.h"

#include "Outbox.h"
#include "Persistence.h"
#include "protocol/client/CharStatusUp.h"
#include "protocol/server/CharStatusUpFail.h"
#include "protocol/server/CharStatusUpSucc.h"
#include "repositories/CharacterRepository.h"
#include "world/Player.h"

#include <optional>
#include <string>

void HandleCharStatusUp(const GameContext& ctx, const CharStatusUp& request, Player& player)
{
    Character& character = player.character;
    const std::optional<std::uint32_t> newValue =
        character.RaiseStat(request.stat_id, request.amount);

    if (!newValue)
    {
        CharStatusUpFail response;
        response.unallocated_point_remaining =
            static_cast<std::int32_t>(character.stats.raw.unallocated_stat_points);
        ctx.outbox.Send(ctx.connection, response);
        return;
    }

    CharStatusUpSucc response;
    response.stat_id = request.stat_id;
    response.current_stat_point = static_cast<std::int32_t>(*newValue);
    response.unallocated_point_remaining =
        static_cast<std::int32_t>(character.stats.raw.unallocated_stat_points);
    auto reply = [ctx, response] { ctx.outbox.Send(ctx.connection, response); };

    // Replies either way: a failed save is rewritten by the next SaveRawStats, or rolls back on relog.
    ctx.persistence.Run(
        [saved = character](IDatabase& db) { CharacterRepository::SaveRawStats(db, saved.id, saved.stats.raw); },
        reply, [reply](const std::string&) { reply(); });
}
