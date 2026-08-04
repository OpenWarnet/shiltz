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

    // Server -> Client
    inline constexpr uint32_t GC_CHAR_DATA_LOAD = 511001;
    inline constexpr uint32_t GC_CRT_LOAD = 511029;
    inline constexpr uint32_t GC_ITEM_MAP_NEW = 511035;
    inline constexpr uint32_t GC_INVENTORY_ITEM_LIST = 511591;
    inline constexpr uint32_t GC_CHAR_EXIT_SUCC = 522010;

    // Resolves an opcode to its constant's name, or "UNKNOWN" if not one of
    // the constants above. Used to make packet capture logs readable.
    std::string_view ToString(uint32_t code);
}
