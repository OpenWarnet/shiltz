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
              << request.creature_instance_id << " unknown: " << request.unknown << "\n";

    auto session = ctx.sessions.Get(ctx.clientSocket);
    if (!session)
    {
        std::cout
            << "Rejecting CG_QUEST_RESULT: socket has no resolved character (never entered)\n";
        return;
    }

    // TODO: quest state/rewards aren't modeled yet -- always grant the same
    // hardcoded reward regardless of which quest/action was reported, on
    // top of the player's running totals (see world/Player.h).
    session->player.money += 1;
    session->player.fame += 1;
    session->player.exp += 100;
    session->player.ap += 1;
    session->player.hp += 1;
    ctx.sessions.Set(ctx.clientSocket, *session);

    PayloadWriter writer;
    QuestSucc response
    {
        .items =
            {
                QuestSuccItem{.inventory_id = 1, 
                              .slot_id = 23,
                              .item_id = 7938,
                              .qty_or_refine = 0,
                              .option = 0x12492497,
                              .option2 = 0,
                              .unknown2 = 0},
                QuestSuccItem{.inventory_id = 1,
                              .slot_id = 24,
                              .item_id = 7938,
                              .qty_or_refine = 0,
                              .option = 0x124924BA,
                              .option2 = 0,
                              .unknown2 = 0},
                QuestSuccItem{.inventory_id = 1,
                              .slot_id = 25,
                              .item_id = 7938,
                              .qty_or_refine = 0,
                              .option = 0x124925DA,
                              .option2 = 0,
                              .unknown2 = 0},
                QuestSuccItem{.inventory_id = 1,
                              .slot_id = 26,
                              .item_id = 7938,
                              .qty_or_refine = 0,
                              .option = 0x125D2492,
                              .option2 = 0,
                              .unknown2 = 0},
                QuestSuccItem{.inventory_id = 1,
                              .slot_id = 27,
                              .item_id = 99,
                              .qty_or_refine = 0,
                              .option = 0x174BFE92,
                              .option2 = 0,
                              .unknown2 = 0},
                QuestSuccItem{.inventory_id = 1,
                              .slot_id = 28,
                              .item_id = 16682,
                              .qty_or_refine = 0,
                              .option = 0x174BFE92,
                              .option2 = 0,
                              .unknown2 = 0},
            },
            .quest_id = 0,
            .money = static_cast<std::uint64_t>(session->player.money),
            .fame = session->player.fame,
            .exp = static_cast<std::uint64_t>(session->player.exp),
            .ap = session->player.ap,
            .hp = session->player.hp,
        };
        std::cout << "Sending GC_QUEST_SUCC: money " << response.money << ", fame " << response.fame
                  << ", exp " << response.exp << ", ap " << response.ap << ", hp " << response.hp
                  << "\n";
        response.Serialize(writer);
        auto data = writer.Data();

        GamePacket packet(GameOpcode::GC_QUEST_SUCC, data);
        auto payload = packet.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, payload);
    }
