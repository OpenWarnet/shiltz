#pragma once

#include <cstdint>
#include <string_view>

namespace GameOpcode
{
    // Client -> Server
    inline constexpr uint32_t CG_MOVE = 411000;
    inline constexpr uint32_t CG_ENTER = 411005;
    inline constexpr uint32_t CG_PLAY_START = 412039;
    inline constexpr uint32_t CG_EXIT = 411007;
    inline constexpr uint32_t CG_ITEM_PICKUP = 411011;
    inline constexpr uint32_t CG_ITEM_DROP = 411012;
    inline constexpr uint32_t CG_ITEM_MOVE = 411013;
    inline constexpr uint32_t CG_QUEST_RESULT = 411026;
    inline constexpr uint32_t CG_ITEM_TRADE_BUY = 411020;
    inline constexpr uint32_t CG_ITEM_TRADE_SELL = 411021;

    // Server -> Client
    inline constexpr uint32_t GC_CHAR_DATA_LOAD = 511001;
    inline constexpr uint32_t GC_ENTER_FAIL = 532050;
    inline constexpr uint32_t GC_CRT_LOAD = 511029;
    inline constexpr uint32_t GC_ITEM_MAP_NEW = 511035;
    inline constexpr uint32_t GC_ITEM_MAP_REMOVE = 511036;
    inline constexpr uint32_t GC_ITEM_PICKUP_SUCC = 521033;
    inline constexpr uint32_t GC_ITEM_DROP_SUCC = 521034;
    // sic -- named "CG_" in the real protocol despite being a server
    // response, matching the client's own (inconsistent) naming.
    inline constexpr uint32_t CG_ITEM_MOVE_SUCC = 521037;
    inline constexpr uint32_t GC_ITEM_MOVE_FAIL = 532102;
    inline constexpr uint32_t GC_INVENTORY_ITEM_LIST = 511591;
    inline constexpr uint32_t GC_CHAR_EXIT_SUCC = 522010;
    inline constexpr uint32_t GC_QUEST_SUCC = 521064;
    inline constexpr uint32_t GC_VIEW_REMOVE_ALL = 511041;
    inline constexpr uint32_t GC_TRADE_BUY_SUCC = 521052;
    inline constexpr uint32_t GC_TRADE_SELL_SUCC = 521054;
    inline constexpr uint32_t GC_TRADE_BUY_FAIL = 531053;
    inline constexpr uint32_t GC_TRADE_SELL_FAIL = 531055;

    // Resolves an opcode to its constant's name, or "UNKNOWN" if not one of
    // the constants above. Used to make packet capture logs readable.
    std::string_view ToString(uint32_t code);
}
