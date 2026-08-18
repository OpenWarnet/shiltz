#pragma once

#include <cstdint>
#include <string_view>

namespace LoginOpcode
{
    enum class Code : uint32_t
    {
        // Client -> Server
        CL_LOGIN = 111000,
        CL_USER_SYSTEM_SPEC_INFO = 111050,
        CL_GAMEGUARD = 111020,
        CL_GET_CHARINFO = 111003,
        CL_DELETE_CHARACTER = 111005,
        CL_CHAR_DELETE_CANCLE = 111012, // sic -- matches wire protocol's own typo
        CL_CREATE_CHARACTER = 111004,
        CL_CREATE_MAP_NUM = 111014,
        CL_GAMESERVER_CONNECT = 111006,

        // Server -> Client
        LC_LOGIN_SUCCESS = 221001,
        LC_LOGIN_FAIL = 231002,
        LC_CHARINFO_SUCCESS = 221003,
        LC_DELETECHAR_SUCCESS = 221007,
        LC_DELETECHAR_FAIL = 231008,
        LC_CHAR_DELETE_CANCLE_SUCCESS = 211018,
        LC_CHAR_DELETE_CANCLE_FAIL = 211019,
        LC_CREATECHAR_SUCCESS = 221005,
        LC_CREATECHAR_FAIL = 231006,
        LC_CREATE_MAP_NUM_SUCCESS = 211022,
        LC_CREATE_MAP_NUM_FAIL = 211023,
        LC_GSERV_CONNECT_SUCCESS = 221009,
    };

    // Brings every enumerator above into LoginOpcode::<name> scope directly,
    // so existing call sites (LoginOpcode::CL_LOGIN, etc.) don't need a
    // LoginOpcode::Code:: qualifier.
    using enum Code;

    // Falls back to "UNKNOWN" for unrecognized opcodes. Used to make packet
    // capture logs readable.
    std::string_view ToString(Code code);
}
