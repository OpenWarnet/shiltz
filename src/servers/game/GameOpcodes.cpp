#include "GameOpcodes.h"

#include <format>

namespace GameOpcode
{
    namespace
    {
        constexpr std::string_view kUnknown = "UNKNOWN";
    }

    std::string_view ToString(Code code)
    {
        // Derives the string from the enumerator token via # so it can never
        // mismatch the name -- add a new opcode to the enum in GameOpcodes.h
        // and one OPCODE_NAME(...) line here.
#define OPCODE_NAME(name) \
    case Code::name:      \
        return #name;

        switch (code)
        {
            OPCODE_NAME(CG_MOVE)
            OPCODE_NAME(CG_ENTER)
            OPCODE_NAME(CG_PLAY_START)
            OPCODE_NAME(CG_EXIT)
            OPCODE_NAME(CG_ITEM_PICKUP)
            OPCODE_NAME(CG_ITEM_DROP)
            OPCODE_NAME(CG_ITEM_MOVE)
            OPCODE_NAME(CG_QUEST_RESULT)
            OPCODE_NAME(CG_ITEM_TRADE_BUY)
            OPCODE_NAME(CG_ITEM_TRADE_SELL)
            OPCODE_NAME(CG_LEVEL_UP_CHECK)
            OPCODE_NAME(CG_CHAR_STATUS_UP)
            OPCODE_NAME(GC_CHAR_SKILL_UP_EX)
            OPCODE_NAME(CG_ITEM_CONFIRM_NPC_REQUEST)
            OPCODE_NAME(CG_STORE_CREATE)
            OPCODE_NAME(CG_STORE_OPEN)
            OPCODE_NAME(CG_STORE_PW_MODIFY)
            OPCODE_NAME(CG_STORE_CLOSE)
            OPCODE_NAME(CG_STORE_ITEM_IN)
            OPCODE_NAME(CG_STORE_ITEM_OUT)
            OPCODE_NAME(CG_STORE_MONEY_IN)
            OPCODE_NAME(CG_STORE_MONEY_OUT)
            OPCODE_NAME(CG_EMOTION)
            OPCODE_NAME(CG_ITEM_DELETE)
            OPCODE_NAME(GC_CHAR_MOVE)
            OPCODE_NAME(GC_CHAR_DATA_LOAD)
            OPCODE_NAME(GC_ENTER_FAIL)
            OPCODE_NAME(GC_CRT_LOAD)
            OPCODE_NAME(GC_CRT_MOVE)
            OPCODE_NAME(GC_ITEM_MAP_NEW)
            OPCODE_NAME(GC_ITEM_MAP_REMOVE)
            OPCODE_NAME(GC_ITEM_PICKUP_SUCC)
            OPCODE_NAME(GC_ITEM_DROP_SUCC)
            OPCODE_NAME(CG_ITEM_MOVE_SUCC)
            OPCODE_NAME(GC_ITEM_MOVE_FAIL)
            OPCODE_NAME(GC_INVENTORY_ITEM_LIST)
            OPCODE_NAME(GC_CHAR_EXIT_SUCC)
            OPCODE_NAME(GC_QUEST_SUCC)
            OPCODE_NAME(GC_QUEST_FAIL)
            OPCODE_NAME(GC_VIEW_REMOVE_ALL)
            OPCODE_NAME(GC_TRADE_BUY_SUCC)
            OPCODE_NAME(GC_TRADE_SELL_SUCC)
            OPCODE_NAME(GC_TRADE_BUY_FAIL)
            OPCODE_NAME(GC_TRADE_SELL_FAIL)
            OPCODE_NAME(GC_LEVEL_UP_SUCC)
            OPCODE_NAME(GC_LEVEL_UP_FAIL)
            OPCODE_NAME(GC_CHAR_STATUS_UP_SUCC)
            OPCODE_NAME(GC_CHAR_STATUS_UP_FAIL)
            OPCODE_NAME(GC_CHAR_SKILL_UP_EX_SUCC)
            OPCODE_NAME(GC_CHAR_SKILL_UP_EX_FAIL)
            OPCODE_NAME(GC_ITEM_CONFIRM_NPC_SUCC)
            OPCODE_NAME(GC_ITEM_CONFIRM_NPC_FAIL)
            OPCODE_NAME(GC_STORE_CREATE_SUCC)
            OPCODE_NAME(GC_STORE_OPEN_SUCC)
            OPCODE_NAME(GC_STORE_OPEN_FAIL)
            OPCODE_NAME(GC_STORE_PW_MODIFY_SUCC)
            OPCODE_NAME(GC_STORE_PW_MODIFY_FAIL)
            OPCODE_NAME(GC_STORE_CLOSE_SUCC)
            OPCODE_NAME(GC_STORE_ITEM_IN)
            OPCODE_NAME(GC_STORE_ITEM_OUT)
            OPCODE_NAME(GC_STORE_MONEY_IN_SUCC)
            OPCODE_NAME(GC_STORE_MONEY_OUT_SUCC)
            OPCODE_NAME(GC_STORE_MONEY_FAIL)
            OPCODE_NAME(GC_EMOTION_SUCC)
            OPCODE_NAME(GC_ITEM_DELETE_SUCC)
            OPCODE_NAME(GC_SERVER_CHANGE)
        default:
            return kUnknown;
        }

#undef OPCODE_NAME
    }

    std::string Describe(Code code)
    {
        const auto name = ToString(code);
        if (name != kUnknown)
            return std::string(name);

        const auto value = static_cast<uint32_t>(code);
        return std::format("0x{:x} / {}", value, value);
    }
}
