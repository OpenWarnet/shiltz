#include "Quest.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "GameSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/QuestResult.h"
#include "protocol/server/QuestSucc.h"
#include "repositories/ItemRepository.h"
#include "storage/Transaction.h"
#include "world/Player.h"

#include <iostream>
#include <vector>

namespace
{
    // Reward items are granted into consecutive wire slots starting here --
    // index i in the vectors below lands on slot kQuestRewardStartSlot + i.
    constexpr std::uint32_t kQuestRewardStartSlot = 23;

    // TODO: quest state/rewards aren't modeled yet -- always grant the same
    // hardcoded item set regardless of which quest/action was reported (see
    // GrantQuestReward for the money/fame/exp/ap/hp side of the same TODO).
    // Every item here is equippable (refine_level, not stackable qty).
    std::vector<Item> BuildQuestRewardItems()
    {
        return {
            Item{.item_id = 7938, .refine_level = 0, .has_refine_level = true,
                         .option_bits = 0x12492497},
            Item{.item_id = 950, // Weapon
                         .refine_level = 0,
                         .has_refine_level = true,
                         .option_bits = 0xFFFFFFFFu},
            //Item{.item_id = 16411, // Head
            //             .refine_level = 0,
            //             .has_refine_level = true,
            //             .option_bits = 0x3FFFFFFF},
            //Item{.item_id = 16628, // Top
            //             .refine_level = 0,
            //             .has_refine_level = true,
            //             .option_bits = 0x3FFFFFFF},
            //Item{.item_id = 16629, // Bot
            //             .refine_level = 0,
            //             .has_refine_level = true,
            //             .option_bits = 0x3FFFFFFF},
            //Item{.item_id = 14337, // Shoes
            //             .refine_level = 0,
            //             .has_refine_level = true,
            //             .option_bits = 0x3FFFFFFF},
        };
    }

    std::vector<QuestSuccItem> ToQuestSuccItems(const std::vector<Item>& items)
    {
        std::vector<QuestSuccItem> result;
        result.reserve(items.size());

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const Item& item = items[i];
            result.push_back(QuestSuccItem{
                .inventory_id = 1,
                .slot_id = kQuestRewardStartSlot + static_cast<std::uint32_t>(i),
                .item_id = item.item_id,
                .qty_or_refine = item.refine_level,
                .option = item.option_bits,
                .unknown2 = 0,
            });
        }

        return result;
    }

    QuestSucc GrantQuestReward(const GameContext& ctx, Player& player)
    {
        player.money += 1;
        player.fame += 1;
        player.exp += 100;
        player.ap += 1;
        player.hp += 1;

        const std::vector<Item> rewardItems = BuildQuestRewardItems();
        const auto characterId = static_cast<std::int64_t>(player.instance_id);

        DatabaseTransaction txn(ctx.db);

        player.SaveMoney(ctx.db);
        player.SaveFame(ctx.db);
        player.SaveLevel(ctx.db); // exp only here -- level is untouched
        player.SaveVitals(ctx.db);

        for (std::size_t i = 0; i < rewardItems.size(); ++i)
        {
            const std::uint32_t slotId = kQuestRewardStartSlot + static_cast<std::uint32_t>(i);
            ItemRepository::SaveItemSlot(ctx.db, characterId, slotId, rewardItems[i]);
            player.SetItemSlot(slotId, rewardItems[i]);
        }

        txn.Commit();

        return QuestSucc{
            .items = ToQuestSuccItems(rewardItems),
            .quest_id = 0,
            .money = static_cast<std::uint64_t>(player.money),
            .fame = player.fame,
            .exp = static_cast<std::uint64_t>(player.exp),
            .ap = player.ap,
            .hp = player.hp,
        };
    }
} // namespace

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

    QuestSucc response = GrantQuestReward(ctx, session->player);
    ctx.sessions.Set(ctx.clientSocket, *session);

    std::cout << "Sending GC_QUEST_SUCC: money " << response.money << ", fame " << response.fame
              << ", exp " << response.exp << ", ap " << response.ap << ", hp " << response.hp
              << "\n";

    PayloadWriter writer;
    response.Serialize(writer);
    auto data = writer.Data();

    GamePacket packet(GameOpcode::GC_QUEST_SUCC, data);
    auto payload = packet.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, payload);
}
