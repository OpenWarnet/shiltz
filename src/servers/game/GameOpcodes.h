#pragma once

#include <cstdint>
#include <string_view>

namespace GameOpcode
{
    enum class Code : uint32_t
    {
        // Client -> Server
        CG_MOVE = 411000,
        CG_ENTER = 411005,
        CG_PLAY_START = 412039,
        CG_EXIT = 411007,
        CG_ITEM_PICKUP = 411011,
        CG_ITEM_DROP = 411012,
        CG_ITEM_MOVE = 411013,
        CG_QUEST_RESULT = 411026,
        CG_ITEM_TRADE_BUY = 411020,
        CG_ITEM_TRADE_SELL = 411021,
        CG_LEVEL_UP_CHECK = 412016,
        CG_CHAR_STATUS_UP = 411018,
        // sic -- named "GC_" in the real protocol despite being a client
        // request, matching the client's own (inconsistent) naming.
        GC_CHAR_SKILL_UP_EX = 411059,
        CG_ITEM_CONFIRM_NPC_REQUEST = 412123, // 0x0649DB
        CG_STORE_CREATE = 411053,
        CG_STORE_OPEN = 411054,
        CG_STORE_PW_MODIFY = 411055,
        CG_STORE_CLOSE = 411056,
        CG_STORE_ITEM_IN = 411057,
        CG_STORE_ITEM_OUT = 411058,
        CG_STORE_MONEY_IN = 411060,
        CG_STORE_MONEY_OUT = 411061,
        CG_EMOTION = 411074,     // 0x0645C2
        CG_ITEM_DELETE = 411454, // 0x06473E

        // Server -> Client
        GC_CHAR_MOVE = 511000,
        GC_CHAR_DATA_LOAD = 511001,
        GC_ENTER_FAIL = 532050,
        GC_CRT_LOAD = 511029,
        GC_ITEM_MAP_NEW = 511035,
        GC_ITEM_MAP_REMOVE = 511036,
        GC_ITEM_PICKUP_SUCC = 521033,
        GC_ITEM_DROP_SUCC = 521034,
        // sic -- named "CG_" in the real protocol despite being a server
        // response, matching the client's own (inconsistent) naming.
        CG_ITEM_MOVE_SUCC = 521037,
        GC_ITEM_MOVE_FAIL = 532102,
        GC_INVENTORY_ITEM_LIST = 511591,
        GC_CHAR_EXIT_SUCC = 522010,
        GC_QUEST_SUCC = 521064,
        GC_QUEST_FAIL = 531065,
        GC_VIEW_REMOVE_ALL = 511041,
        GC_TRADE_BUY_SUCC = 521052,
        GC_TRADE_SELL_SUCC = 521054,
        GC_TRADE_BUY_FAIL = 531053,
        GC_TRADE_SELL_FAIL = 531055,
        GC_LEVEL_UP_SUCC = 521045,
        GC_LEVEL_UP_FAIL = 531066,
        GC_CHAR_STATUS_UP_SUCC = 521048,
        GC_CHAR_STATUS_UP_FAIL = 531049,
        GC_CHAR_SKILL_UP_EX_SUCC = 521100,
        GC_CHAR_SKILL_UP_EX_FAIL = 531102,
        GC_ITEM_CONFIRM_NPC_SUCC = 531234, // 0x081B22
        GC_ITEM_CONFIRM_NPC_FAIL = 531235, // 0x081B23
        GC_STORE_CREATE_SUCC = 521111,
        GC_STORE_OPEN_SUCC = 521113,
        GC_STORE_OPEN_FAIL = 531114,
        GC_STORE_PW_MODIFY_SUCC = 521115,
        GC_STORE_PW_MODIFY_FAIL = 531116,
        GC_STORE_CLOSE_SUCC = 521117,
        GC_STORE_ITEM_IN = 521118,
        GC_STORE_ITEM_OUT = 521119,
        GC_STORE_MONEY_IN_SUCC = 521122,
        GC_STORE_MONEY_OUT_SUCC = 521123,
        GC_STORE_MONEY_FAIL = 531124,
        GC_EMOTION_SUCC = 521464,     // 0x07F4F8
        GC_ITEM_DELETE_SUCC = 521462, // 0x07F4F6
        GC_SERVER_CHANGE = 511086,    // 0x7CC6E
    };

    // Brings every enumerator above into GameOpcode::<name> scope directly,
    // so existing call sites (GameOpcode::CG_ENTER, etc.) don't need a
    // GameOpcode::Code:: qualifier.
    using enum Code;

    // Resolves an opcode to its constant's name, or "UNKNOWN" if not one of
    // the constants above. Used to make packet capture logs readable.
    std::string_view ToString(Code code);
}
