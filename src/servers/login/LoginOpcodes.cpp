#include "LoginOpcodes.h"

namespace LoginOpcode
{
    std::string_view ToString(uint32_t code)
    {
        switch (code)
        {
        case CL_LOGIN:
            return "CL_LOGIN";
        case CL_USER_SYSTEM_SPEC_INFO:
            return "CL_USER_SYSTEM_SPEC_INFO";
        case CL_GAMEGUARD:
            return "CL_GAMEGUARD";
        case CL_GET_CHARINFO:
            return "CL_GET_CHARINFO";
        case CL_DELETE_CHARACTER:
            return "CL_DELETE_CHARACTER";
        case CL_CHAR_DELETE_CANCLE:
            return "CL_CHAR_DELETE_CANCLE";
        case CL_CREATE_CHARACTER:
            return "CL_CREATE_CHARACTER";
        case CL_CREATE_MAP_NUM:
            return "CL_CREATE_MAP_NUM";
        case CL_GAMESERVER_CONNECT:
            return "CL_GAMESERVER_CONNECT";
        case LC_LOGIN_SUCCESS:
            return "LC_LOGIN_SUCCESS";
        case LC_LOGIN_FAIL:
            return "LC_LOGIN_FAIL";
        case LC_CHARINFO_SUCCESS:
            return "LC_CHARINFO_SUCCESS";
        case LC_DELETECHAR_SUCCESS:
            return "LC_DELETECHAR_SUCCESS";
        case LC_CHAR_DELETE_CANCLE_SUCCESS:
            return "LC_CHAR_DELETE_CANCLE_SUCCESS";
        case LC_CREATECHAR_SUCCESS:
            return "LC_CREATECHAR_SUCCESS";
        case LC_CREATE_MAP_NUM_SUCCESS:
            return "LC_CREATE_MAP_NUM_SUCCESS";
        case LC_GSERV_CONNECT_SUCCESS:
            return "LC_GSERV_CONNECT_SUCCESS";
        default:
            return "UNKNOWN";
        }
    }
}
