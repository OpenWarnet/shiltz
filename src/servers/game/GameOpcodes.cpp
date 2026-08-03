#include "GameOpcodes.h"

namespace GameOpcode
{
    std::string_view ToString(uint32_t code)
    {
        switch (code)
        {
        case CG_ENTER:
            return "CG_ENTER";
        case CG_PLAY_START:
            return "CG_PLAY_START";
        case CG_EXIT:
            return "CG_EXIT";
        case GC_CHAR_DATA_LOAD:
            return "GC_CHAR_DATA_LOAD";
        case GC_INVENTORY_ITEM_LIST:
            return "GC_INVENTORY_ITEM_LIST";
        case GC_CRT_LOAD:
            return "GC_CRT_LOAD";
        case GC_CHAR_EXIT_SUCC:
            return "GC_CHAR_EXIT_SUCC";
        default:
            return "UNKNOWN";
        }
    }
}
