#include "Quest.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/QuestResult.h"
#include "protocol/server/QuestSucc.h"

#include <iostream>

void HandleQuestResult(const GameContext& ctx, const QuestResult& request)
{
    std::cout << "Quest result: action_id " << request.action_id << ", creature_instance_id "
              << request.creature_instance_id << "\n";

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout << "Rejecting CG_QUEST_RESULT: socket has no resolved character (never entered)\n";
        return;
    }

    // TODO: quest state/rewards aren't modeled yet -- always grant the same
    // hardcoded reward regardless of which quest/action was reported, on
    // top of the player's running totals (see world/Player.h).
    session->player.money += 1000890;
    session->player.fame += 500;
    session->player.exp += 10000;
    session->player.ap += 456;
    session->player.hp += 123456;
    ctx.sessions.Set(ctx.clientSocket, *session);

    PayloadWriter writer;
    QuestSucc response{
        .items = {QuestSuccItem{
            .instance_id = 0, .item_id = 41, .qty_or_refine = 2, .option = 0, .option2 = 0, .time = 0}},
        .quest_id = 0,
        .money = static_cast<std::uint32_t>(session->player.money),
        .fame = session->player.fame,
        .exp = static_cast<std::uint32_t>(session->player.exp),
        .ap = session->player.ap,
        .hp = session->player.hp,
    };
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket packet(GameOpcode::GC_QUEST_SUCC, data);
    auto payload = packet.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, payload);
}
