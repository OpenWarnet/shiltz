#pragma once

#include <cstdint>

namespace GameOpcode
{
    // Client -> Server
    inline constexpr uint32_t CG_ENTER = 411005;
    inline constexpr uint32_t CG_PLAY_START = 412039;
    inline constexpr uint32_t CG_EXIT = 411007;

    // Server -> Client
    inline constexpr uint32_t GC_CHAR_DATA_LOAD = 511001;
    inline constexpr uint32_t GC_INVENTORY_ITEM_LIST = 511591;
    inline constexpr uint32_t GC_CRT_LOAD = 511029;
    inline constexpr uint32_t GC_CHAR_EXIT_SUCC = 522010;
}
