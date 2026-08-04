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
        case CG_ITEM_PICKUP:
            return "CG_ITEM_PICKUP";
        case CG_ITEM_DROP:
            return "CG_ITEM_DROP";
        case CG_ITEM_MOVE:
            return "CG_ITEM_MOVE";
        case CG_ITEM_MOVE_SUCC:
            return "CG_ITEM_MOVE_SUCC";
        case GC_ITEM_MOVE_FAIL:
            return "GC_ITEM_MOVE_FAIL";
        case GC_CHAR_DATA_LOAD:
            return "GC_CHAR_DATA_LOAD";
        case GC_ENTER_FAIL:
            return "GC_ENTER_FAIL";
        case GC_ITEM_MAP_REMOVE:
            return "GC_ITEM_MAP_REMOVE";
        case GC_ITEM_PICKUP_SUCC:
            return "GC_ITEM_PICKUP_SUCC";
        case GC_ITEM_DROP_SUCC:
            return "GC_ITEM_DROP_SUCC";
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
