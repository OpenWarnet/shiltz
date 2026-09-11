#include "ClientProtocol.h"

#include "GameContext.h"
#include "GameOpcodes.h"
#include "GamePacket.h"
#include "world/World.h"
#include "common/PacketCapture.h"
#include "common/PayloadReader.h"
#include "protocol/client/CharMove.h"
#include "protocol/client/CharSkillUpEx.h"
#include "protocol/client/CharStatusUp.h"
#include "protocol/client/Emotion.h"
#include "protocol/client/GameEnter.h"
#include "protocol/client/GameExit.h"
#include "protocol/client/ItemConfirmNpcRequest.h"
#include "protocol/client/ItemDelete.h"
#include "protocol/client/ItemDrop.h"
#include "protocol/client/ItemMove.h"
#include "protocol/client/ItemPickup.h"
#include "protocol/client/ItemTradeBuy.h"
#include "protocol/client/ItemTradeSell.h"
#include "protocol/client/LevelUpCheck.h"
#include "protocol/client/PlayStart.h"
#include "protocol/client/QuestResult.h"
#include "protocol/client/StoreClose.h"
#include "protocol/client/StoreCreate.h"
#include "protocol/client/StoreItemIn.h"
#include "protocol/client/StoreItemOut.h"
#include "protocol/client/StoreMoneyIn.h"
#include "protocol/client/StoreMoneyOut.h"
#include "protocol/client/StoreOpen.h"
#include "protocol/client/StorePwModify.h"

#include <concepts>
#include <format>
#include <iostream>
#include <memory>

namespace
{
    template <typename T>
        requires std::derived_from<T, ClientProtocol>
    std::unique_ptr<ClientProtocol> DecodeAs(const GamePacket& packet)
    {
        PacketCapture::LogHandled(PacketCapture::Direction::Inbound,
                                  static_cast<std::uint32_t>(packet.GetCode()),
                                  GameOpcode::ToString(packet.GetCode()), packet.GetPayload());

        PayloadReader reader(packet.GetPayload());
        auto message = std::make_unique<T>();
        if (!message->Deserialize(reader))
        {
            std::cout << std::format("!! {} : dropped, failed to parse payload\n",
                                     GameOpcode::Describe(packet.GetCode()));
            return nullptr;
        }

        return message;
    }
} // namespace

void PlayerMessage::Handle(const GameContext& ctx) const
{
    Player* player = ctx.world.FindPlayer(ctx.connection);
    if (!player)
        return;

    Handle(ctx, *player);
}

std::unique_ptr<ClientProtocol> ClientProtocol::Create(const GamePacket& packet)
{
    switch (packet.GetCode())
    {
    case GameOpcode::CG_ENTER:
        return DecodeAs<GameEnter>(packet);
    case GameOpcode::CG_PLAY_START:
        return DecodeAs<PlayStart>(packet);
    case GameOpcode::CG_EXIT:
        return DecodeAs<GameExit>(packet);
    case GameOpcode::CG_MOVE:
        return DecodeAs<CharMove>(packet);
    case GameOpcode::CG_ITEM_PICKUP:
        return DecodeAs<ItemPickup>(packet);
    case GameOpcode::CG_ITEM_MOVE:
        return DecodeAs<ItemMove>(packet);
    case GameOpcode::CG_ITEM_DROP:
        return DecodeAs<ItemDrop>(packet);
    case GameOpcode::CG_ITEM_DELETE:
        return DecodeAs<ItemDelete>(packet);
    case GameOpcode::CG_QUEST_RESULT:
        return DecodeAs<QuestResult>(packet);
    case GameOpcode::CG_ITEM_TRADE_BUY:
        return DecodeAs<ItemTradeBuy>(packet);
    case GameOpcode::CG_ITEM_TRADE_SELL:
        return DecodeAs<ItemTradeSell>(packet);
    case GameOpcode::CG_LEVEL_UP_CHECK:
        return DecodeAs<LevelUpCheck>(packet);
    case GameOpcode::CG_CHAR_STATUS_UP:
        return DecodeAs<CharStatusUp>(packet);
    case GameOpcode::GC_CHAR_SKILL_UP_EX:
        return DecodeAs<CharSkillUpEx>(packet);
    case GameOpcode::CG_ITEM_CONFIRM_NPC_REQUEST:
        return DecodeAs<ItemConfirmNpcRequest>(packet);
    case GameOpcode::CG_STORE_CREATE:
        return DecodeAs<StoreCreate>(packet);
    case GameOpcode::CG_STORE_OPEN:
        return DecodeAs<StoreOpen>(packet);
    case GameOpcode::CG_STORE_PW_MODIFY:
        return DecodeAs<StorePwModify>(packet);
    case GameOpcode::CG_STORE_CLOSE:
        return DecodeAs<StoreClose>(packet);
    case GameOpcode::CG_STORE_ITEM_IN:
        return DecodeAs<StoreItemIn>(packet);
    case GameOpcode::CG_STORE_ITEM_OUT:
        return DecodeAs<StoreItemOut>(packet);
    case GameOpcode::CG_STORE_MONEY_IN:
        return DecodeAs<StoreMoneyIn>(packet);
    case GameOpcode::CG_STORE_MONEY_OUT:
        return DecodeAs<StoreMoneyOut>(packet);
    case GameOpcode::CG_EMOTION:
        return DecodeAs<Emotion>(packet);
    default:
        PacketCapture::LogUnhandled(static_cast<std::uint32_t>(packet.GetCode()),
                                    GameOpcode::ToString(packet.GetCode()), packet.GetPayload());
        std::cout << std::format("!! {} : dropped, no handler\n",
                                 GameOpcode::Describe(packet.GetCode()));
        return nullptr;
    }
}
