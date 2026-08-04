#include "Movement.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/CharMove.h"
#include "protocol/server/ItemMapNew.h"
#include "world/Item.h"
#include "world/World.h"

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

    // TODO: CharMove carries no item_id -- kept as the prior hardcoded
    // placeholder pending a real item-spawn source.
    constexpr std::uint32_t kItemId = 1;

    ctx.world.GetMap().AddItem(Item{
        .id = random_val,
        .item_id = kItemId,
        .x = request.x,
        .y = request.y,
        .quantity = 1,
    });

    ItemMapNew response{
        .id = random_val,
        .x = request.x,
        .y = request.y,
        .item_id = kItemId,
        .owner_id = request.user_id,
    };
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket responsePacket(GameOpcode::GC_ITEM_MAP_NEW, data);
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);
}
