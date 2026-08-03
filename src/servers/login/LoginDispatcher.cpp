#include "LoginDispatcher.h"

#include "LoginOpcodes.h"
#include "LoginPacket.h"
#include "common/OpcodeBinder.h"
#include "handlers/Auth.h"
#include "handlers/Character.h"
#include "handlers/GameHandover.h"
#include "protocol/client/CreateCharacter.h"
#include "protocol/client/GameConnect.h"
#include "protocol/client/Login.h"
#include "protocol/client/ServerSelect.h"
#include "protocol/client/SetCharacterMap.h"
#include "protocol/shared/GenericCharacterPayload.h"

namespace
{
    auto When(uint32_t opcode) { return OpcodeBinder<LoginContext, LoginPacket>(opcode); }
}

LoginDispatcher::LoginDispatcher()
    : Dispatcher{
          &LoginOpcode::ToString,
          {
              When(LoginOpcode::CL_LOGIN).ParseAs<Login>().Then(HandleClLogin),
              When(LoginOpcode::CL_USER_SYSTEM_SPEC_INFO)
                  .SkipParse(SkipReason::Ignored)
                  .Then(HandleClUserSystemSpecInfo),
              When(LoginOpcode::CL_GAMEGUARD).SkipParse(SkipReason::Ignored).Then(HandleClGameguard),
              When(LoginOpcode::CL_GET_CHARINFO).ParseAs<ServerSelect>().Then(HandleClGetCharinfo),
              When(LoginOpcode::CL_DELETE_CHARACTER)
                  .ParseAs<GenericCharacterPayload>()
                  .Then(HandleClDeleteCharacter),
              When(LoginOpcode::CL_CHAR_DELETE_CANCLE)
                  .ParseAs<GenericCharacterPayload>()
                  .Then(HandleClCharDeleteCancle),
              When(LoginOpcode::CL_CREATE_CHARACTER)
                  .ParseAs<CreateCharacter>()
                  .Then(HandleClCreateCharacter),
              When(LoginOpcode::CL_CREATE_MAP_NUM)
                  .ParseAs<SetCharacterMap>()
                  .Then(HandleClCreateMapNum),
              When(LoginOpcode::CL_GAMESERVER_CONNECT)
                  .ParseAs<GameConnect>()
                  .Then(HandleClGameserverConnect),
          },
      }
{
}
