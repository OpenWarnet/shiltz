#include "CharStatus.h"

#include "Outbox.h"
#include "Persistence.h"
#include "enums/StatId.h"
#include "protocol/client/CharStatusUp.h"
#include "protocol/server/CharStatusUpFail.h"
#include "protocol/server/CharStatusUpSucc.h"
#include "tables/GameData.h"
#include "world/Player.h"
#include "stats/Stats.h"

#include <string>

namespace
{
    // Returns nullptr for an out-of-range stat_id.
    std::uint32_t* ResolveRawStat(CharacterRawStats& raw, std::int32_t statId)
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

void HandleCharStatusUp(const GameContext& ctx, const CharStatusUp& request, Player& player)
{
    Character& character = player.character;
    CharacterRawStats& raw = character.stats.raw;
    std::uint32_t* rawStat = ResolveRawStat(raw, request.stat_id);
    const bool canAfford =
        rawStat && request.amount > 0 &&
        raw.unallocated_stat_points >= static_cast<std::uint32_t>(request.amount);

    if (!canAfford)
    {
        CharStatusUpFail response;
        response.unallocated_point_remaining =
            static_cast<std::int32_t>(raw.unallocated_stat_points);
        ctx.outbox.Send(ctx.connection, response);
        return;
    }

    *rawStat += static_cast<std::uint32_t>(request.amount);
    raw.unallocated_stat_points -= static_cast<std::uint32_t>(request.amount);
    RecalculateDerivedStats(character, ctx.data.items, ctx.data.setOptions, ctx.data.statusRates);

    CharStatusUpSucc response;
    response.stat_id = request.stat_id;
    response.current_stat_point = static_cast<std::int32_t>(*rawStat);
    response.unallocated_point_remaining = static_cast<std::int32_t>(raw.unallocated_stat_points);
    auto reply = [ctx, response] { ctx.outbox.Send(ctx.connection, response); };

    // Replies either way: a failed save is rewritten by the next SaveRawStats, or rolls back on relog.
    ctx.persistence.Run([saved = character](IDatabase& db) { saved.SaveRawStats(db); }, reply,
                        [reply](const std::string&) { reply(); });
}
