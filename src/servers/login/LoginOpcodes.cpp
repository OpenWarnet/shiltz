#include "LoginOpcodes.h"

namespace LoginOpcode
{
    std::string_view ToString(Code code)
    {
        // Derives the string from the enumerator token via # so it can never
        // mismatch the name -- add a new opcode to the enum in
        // LoginOpcodes.h and one OPCODE_NAME(...) line here.
#define OPCODE_NAME(name) \
    case Code::name:      \
        return #name;

        switch (code)
        {
            OPCODE_NAME(CL_LOGIN)
            OPCODE_NAME(CL_USER_SYSTEM_SPEC_INFO)
            OPCODE_NAME(CL_GAMEGUARD)
            OPCODE_NAME(CL_GET_CHARINFO)
            OPCODE_NAME(CL_DELETE_CHARACTER)
            OPCODE_NAME(CL_CHAR_DELETE_CANCLE)
            OPCODE_NAME(CL_CREATE_CHARACTER)
            OPCODE_NAME(CL_CREATE_MAP_NUM)
            OPCODE_NAME(CL_GAMESERVER_CONNECT)
            OPCODE_NAME(LC_LOGIN_SUCCESS)
            OPCODE_NAME(LC_LOGIN_FAIL)
            OPCODE_NAME(LC_CHARINFO_SUCCESS)
            OPCODE_NAME(LC_DELETECHAR_SUCCESS)
            OPCODE_NAME(LC_DELETECHAR_FAIL)
            OPCODE_NAME(LC_CHAR_DELETE_CANCLE_SUCCESS)
            OPCODE_NAME(LC_CHAR_DELETE_CANCLE_FAIL)
            OPCODE_NAME(LC_CREATECHAR_SUCCESS)
            OPCODE_NAME(LC_CREATECHAR_FAIL)
            OPCODE_NAME(LC_CREATE_MAP_NUM_SUCCESS)
            OPCODE_NAME(LC_CREATE_MAP_NUM_FAIL)
            OPCODE_NAME(LC_GSERV_CONNECT_SUCCESS)
        default:
            return "UNKNOWN";
        }

#undef OPCODE_NAME
    }
}
