#pragma once

#include <cstdint>

namespace LoginOpcode
{
    // Client -> Server
    inline constexpr uint32_t CL_LOGIN = 111000;
    inline constexpr uint32_t CL_USER_SYSTEM_SPEC_INFO = 111050;
    inline constexpr uint32_t CL_GAMEGUARD = 111020;
    inline constexpr uint32_t CL_GET_CHARINFO = 111003;
    inline constexpr uint32_t CL_DELETE_CHARACTER = 111005;
    inline constexpr uint32_t CL_CHAR_DELETE_CANCLE = 111012; // sic -- matches wire protocol's own typo
    inline constexpr uint32_t CL_CREATE_CHARACTER = 111004;
    inline constexpr uint32_t CL_CREATE_MAP_NUM = 111014;
    inline constexpr uint32_t CL_GAMESERVER_CONNECT = 111006;

    // Server -> Client
    inline constexpr uint32_t LC_LOGIN_SUCCESS = 221001;
    inline constexpr uint32_t LC_LOGIN_FAIL = 231002;
    inline constexpr uint32_t LC_CHARINFO_SUCCESS = 221003;
    inline constexpr uint32_t LC_DELETECHAR_SUCCESS = 221007;
    inline constexpr uint32_t LC_CHAR_DELETE_CANCLE_SUCCESS = 211018;
    inline constexpr uint32_t LC_CREATECHAR_SUCCESS = 221005;
    inline constexpr uint32_t LC_CREATE_MAP_NUM_SUCCESS = 211022;
    inline constexpr uint32_t LC_GSERV_CONNECT_SUCCESS = 221009;
    }
