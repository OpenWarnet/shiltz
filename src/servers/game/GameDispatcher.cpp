#include "GameDispatcher.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "common/OpcodeBinder.h"
#include "handlers/Session.h"
#include "handlers/Movement.h"
#include "handlers/Inventory.h"
#include "handlers/Quest.h"
#include "handlers/Trade.h"
#include "handlers/LevelUp.h"
#include "handlers/CharStatus.h"
#include "handlers/CharSkillUp.h"
#include "handlers/ItemConfirmNpc.h"
#include "handlers/Store.h"
#include "protocol/client/GameEnter.h"
#include "protocol/client/CharMove.h"
#include "protocol/client/ItemPickup.h"
#include "protocol/client/ItemMove.h"
#include "protocol/client/ItemDrop.h"
#include "protocol/client/ItemTradeBuy.h"
#include "protocol/client/ItemTradeSell.h"
#include "protocol/client/QuestResult.h"
#include "protocol/client/LevelUpCheck.h"
#include "protocol/client/CharStatusUp.h"
#include "protocol/client/CharSkillUpEx.h"
#include "protocol/client/ItemConfirmNpcRequest.h"
#include "protocol/client/StoreOpen.h"
#include "protocol/client/StorePwModify.h"
#include "protocol/client/StoreClose.h"
#include "protocol/client/StoreItemIn.h"
#include "protocol/client/StoreItemOut.h"
#include "protocol/client/StoreMoneyIn.h"
#include "protocol/client/StoreMoneyOut.h"

namespace
{
    auto When(uint32_t opcode) { return OpcodeBinder<GameContext, GamePacket>(opcode); }
}

GameDispatcher::GameDispatcher()
    : Dispatcher{
          &GameOpcode::ToString,
          {
              When(GameOpcode::CG_ENTER).ParseAs<GameEnter>().Then(HandleEnter),
              When(GameOpcode::CG_PLAY_START).SkipParse(SkipReason::Ignored).Then(HandleCgPlayStart),
              When(GameOpcode::CG_EXIT).SkipParse(SkipReason::Empty).Then(HandleCgExit),
              When(GameOpcode::CG_MOVE).ParseAs<CharMove>().Then(HandleMovement),
              When(GameOpcode::CG_ITEM_PICKUP).ParseAs<ItemPickup>().Then(HandleItemPickup),
              When(GameOpcode::CG_ITEM_MOVE).ParseAs<ItemMove>().Then(HandleItemMove),
              When(GameOpcode::CG_ITEM_DROP).ParseAs<ItemDrop>().Then(HandleItemDrop),
              When(GameOpcode::CG_QUEST_RESULT).ParseAs<QuestResult>().Then(HandleQuestResult),
              When(GameOpcode::CG_ITEM_TRADE_BUY).ParseAs<ItemTradeBuy>().Then(HandleItemTradeBuy),
              When(GameOpcode::CG_ITEM_TRADE_SELL).ParseAs<ItemTradeSell>().Then(HandleItemTradeSell),
              When(GameOpcode::CG_LEVEL_UP_CHECK).ParseAs<LevelUpCheck>().Then(HandleLevelUpCheck),
              When(GameOpcode::CG_CHAR_STATUS_UP).ParseAs<CharStatusUp>().Then(HandleCharStatusUp),
              When(GameOpcode::GC_CHAR_SKILL_UP_EX).ParseAs<CharSkillUpEx>().Then(HandleCharSkillUpEx),
              When(GameOpcode::CG_ITEM_CONFIRM_NPC_REQUEST)
                  .ParseAs<ItemConfirmNpcRequest>()
                  .Then(HandleItemConfirmNpcRequest),
              When(GameOpcode::CG_STORE_OPEN).ParseAs<StoreOpen>().Then(HandleStoreOpen),
              When(GameOpcode::CG_STORE_PW_MODIFY).ParseAs<StorePwModify>().Then(HandleStorePwModify),
              When(GameOpcode::CG_STORE_CLOSE).ParseAs<StoreClose>().Then(HandleStoreClose),
              When(GameOpcode::CG_STORE_ITEM_IN).ParseAs<StoreItemIn>().Then(HandleStoreItemIn),
              When(GameOpcode::CG_STORE_ITEM_OUT).ParseAs<StoreItemOut>().Then(HandleStoreItemOut),
              When(GameOpcode::CG_STORE_MONEY_IN).ParseAs<StoreMoneyIn>().Then(HandleStoreMoneyIn),
              When(GameOpcode::CG_STORE_MONEY_OUT).ParseAs<StoreMoneyOut>().Then(HandleStoreMoneyOut),
          },
      }
{
}
