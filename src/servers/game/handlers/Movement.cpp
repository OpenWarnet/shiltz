#include "Movement.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/Server.h"
#include "protocol/client/CharMove.h"
#include "protocol/server/CharMoveUpdate.h"
#include "protocol/server/CrtLoad.h"
#include "protocol/server/ViewRemoveAll.h"
#include "tables/GameData.h"
#include "tables/MonsterTable.h"
#include "world/World.h"

#include <algorithm>
#include <iostream>

namespace
{
    bool Contains(const std::vector<std::pair<std::int32_t, std::int32_t>>& zones,
                  const std::pair<std::int32_t, std::int32_t>& zone)
    {
        return std::find(zones.begin(), zones.end(), zone) != zones.end();
    }
} // namespace

void HandleMovement(const GameContext& ctx, const CharMove& request)
{
    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_MOVE: socket has no resolved character (never entered)\n";
        return;
    }

    // Null if session->player.map_id isn't a map.scr id this World loaded --
    // zone/creature updates stay empty in that case (see Session.cpp's
    // HandleEnter for the same guard on the initial CG_ENTER).
    Map* map = ctx.world.GetMap(session->player.map_id);
    const auto newZones = map ? map->ZonesAround(static_cast<std::int32_t>(request.x),
                                                  static_cast<std::int32_t>(request.y))
                               : std::vector<std::pair<std::int32_t, std::int32_t>>{};
    const auto& oldZones = session->player.known_zones;

    CrtLoad crtLoadResponse;
    for (const auto& zone : newZones)
    {
        if (Contains(oldZones, zone))
            continue;

        for (const auto& creature : map->CreaturesInZone(zone.first, zone.second))
        {
            const MonsterRecord* monsterRecord = ctx.data.monsters.Find(creature.monster_id);

            std::cout << "Loading creature " << creature.instance_id << " (monster_id "
                      << creature.monster_id << ") at (" << creature.x << ", " << creature.y
                      << ") for player " << session->characterId << "\n";

            crtLoadResponse.records.push_back(CrtLoadRecord{
                .id = creature.instance_id,
                .x = static_cast<std::uint32_t>(creature.x),
                .y = static_cast<std::uint32_t>(creature.y),
                .monster_id = static_cast<std::uint32_t>(creature.monster_id),
                .direction = static_cast<std::uint32_t>(creature.direction),
                .hp = monsterRecord ? static_cast<std::uint64_t>(monsterRecord->hp) : 0,
            });
        }
    }

    if (!crtLoadResponse.records.empty())
    {
        PayloadWriter writer;
        crtLoadResponse.Serialize(writer);
        auto data = writer.Data();

        GamePacket packet(GameOpcode::GC_CRT_LOAD, data);
        auto payload = packet.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, payload);
    }

    ViewRemoveAll removeResponse;
    for (const auto& zone : oldZones)
    {
        if (Contains(newZones, zone))
            continue;

        for (const auto& creature : map->CreaturesInZone(zone.first, zone.second))
        {
            removeResponse.creature_ids.push_back(creature.instance_id);
        }
    }

    if (!removeResponse.creature_ids.empty())
    {
        PayloadWriter writer;
        removeResponse.Serialize(writer);
        auto data = writer.Data();

        GamePacket packet(GameOpcode::GC_VIEW_REMOVE_ALL, data);
        auto payload = packet.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, payload);
    }

    session->player.x = static_cast<std::int32_t>(request.x);
    session->player.y = static_cast<std::int32_t>(request.y);
    session->player.direction = static_cast<std::int32_t>(request.move_direction);
    session->player.known_zones = newZones;

    ctx.sessions.Set(ctx.clientSocket, *session);

    // Speed is this connection's own derived stat, not an echo of
    // CharMove::speed -- see protocol/server/CharMoveUpdate.h.
    CharMoveUpdate moveResponse{
        .user_instance_id = session->player.instance_id,
        .direction = request.move_direction,
        .x = request.x,
        .y = request.y,
        .speed = static_cast<std::uint32_t>(request.speed),
        .stop_direction = static_cast<std::uint32_t>(request.stop_direction),
    };

    PayloadWriter moveWriter;
    moveResponse.Serialize(moveWriter);

    GamePacket movePacket(GameOpcode::GC_CHAR_MOVE, moveWriter.Data());
    ctx.server.SendTo(ctx.clientSocket, movePacket.Serialize(ctx.key));
}
