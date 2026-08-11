#include "CharStatus.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/CharStatusUp.h"
#include "protocol/server/CharStatusUpFail.h"
#include "protocol/server/CharStatusUpSucc.h"
#include "world/Player.h"

#include <iostream>

namespace
{
    // stat_id -> PlayerRawStats field, using the client's own wire order
    // (see CharacterDataLoad's stats_str..stats_sen fields), 1-based:
    // 1=str, 2=int, 3=dex, 4=con, 5=men, 6=sen. Returns nullptr for an
    // out-of-range id.
    std::uint32_t* ResolveRawStat(PlayerRawStats& raw, std::int32_t statId)
    {
        switch (statId)
        {
        case 1: // str
            return &raw.strength;
        case 2: // int
            return &raw.intelligence;
        case 3: // dex
            return &raw.dexterity;
        case 4: // con
            return &raw.constitution;
        case 5: // men
            return &raw.mentality;
        case 6: // sen
            return &raw.sense;
        default:
            return nullptr;
        }
    }
}

void HandleCharStatusUp(const GameContext& ctx, const CharStatusUp& request)
{
    std::cout << "Char status up: stat_id " << request.stat_id << ", amount " << request.amount
              << "\n";

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout
            << "Rejecting CG_CHAR_STATUS_UP: socket has no resolved character (never entered)\n";
        return;
    }

    PlayerRawStats& raw = session->player.stats.raw;
    std::uint32_t* rawStat = ResolveRawStat(raw, request.stat_id);
    const bool canAfford =
        rawStat && request.amount > 0 &&
        raw.unallocated_stat_points >= static_cast<std::uint32_t>(request.amount);

    PayloadWriter writer;

    if (canAfford)
    {
        *rawStat += static_cast<std::uint32_t>(request.amount);
        raw.unallocated_stat_points -= static_cast<std::uint32_t>(request.amount);
        ctx.sessions.Set(ctx.clientSocket, *session);
        session->player.SaveRawStats(ctx.db);

        CharStatusUpSucc response{
            .stat_id = request.stat_id,
            .current_stat_point = static_cast<std::int32_t>(*rawStat),
            .unallocated_point_remaining = static_cast<std::int32_t>(raw.unallocated_stat_points),
        };
        std::cout << "Sending GC_CHAR_STATUS_UP_SUCC: stat_id " << response.stat_id
                  << ", current_stat_point " << response.current_stat_point
                  << ", unallocated_point_remaining " << response.unallocated_point_remaining
                  << "\n";
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_CHAR_STATUS_UP_SUCC, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    }
    else
    {
        CharStatusUpFail response{
            .unallocated_point_remaining = static_cast<std::int32_t>(raw.unallocated_stat_points),
        };
        std::cout << "Sending GC_CHAR_STATUS_UP_FAIL: unallocated_point_remaining "
                  << response.unallocated_point_remaining << "\n";
        response.Serialize(writer);

        GamePacket packet(GameOpcode::GC_CHAR_STATUS_UP_FAIL, writer.Data());
        ctx.server.SendTo(ctx.clientSocket, packet.Serialize(ctx.key));
    }
}
