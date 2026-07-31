#include "GameDispatcher.h"

#include "GameOpcodes.h"
#include "GamePacket.h"
#include "common/OpcodeBinder.h"
#include "handlers/Session.h"
#include "protocol/client/GameEnter.h"

namespace
{
    auto When(uint32_t opcode) { return OpcodeBinder<GameContext, GamePacket>(opcode); }
}

GameDispatcher::GameDispatcher()
    : Dispatcher{
          When(GameOpcode::CG_ENTER).ParseAs<GameEnter>().Then(HandleCgEnter),
          When(GameOpcode::CG_PLAY_START).SkipParse(SkipReason::Ignored).Then(HandleCgPlayStart),
          When(GameOpcode::CG_EXIT).SkipParse(SkipReason::Empty).Then(HandleCgExit),
      }
{
}
