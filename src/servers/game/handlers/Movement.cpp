#include "Movement.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/CharMove.h"
#include "protocol/server/ItemMapNew.h"

#include <iostream>
#include <random>
#include <cstdint>

void HandleMovement(const GameContext& ctx, const CharMove& request)
{
    PayloadWriter writer;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> distrib(1, 1000);
    uint32_t random_val = distrib(gen);

    ItemMapNew response{
        .id = random_val,
        .x = request.x,
        .y = request.y,
        .item_id = 1,
        .owner_id = request.user_id,
    };
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket responsePacket(GameOpcode::GC_ITEM_MAP_NEW, data);
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);
}
