#pragma once

#include "ItemTable.h"
#include "LevelTable.h"
#include "MonsterTable.h"
#include "QuestTable.h"
#include "SellerTable.h"
#include "SetOptionTable.h"
#include "SkillTable.h"
#include "StatusTable.h"
#include "WarpTable.h"

// Owns every static, .scr-derived game-data table -- items, monsters,
// sellers, set-option bonuses, level exp requirements, skills, status.scr
// rates, and warp destinations. Loaded once at process startup (see Load())
// and never mutated afterward, so callers only ever need a `const
// GameData&`.
class GameData
{
public:
    // Loads every table from its file(s) under world/data. Throws
    // std::runtime_error if any file can't be opened.
    void Load();

    ItemTable items;
    MonsterTable monsters;
    SellerTable sellers;
    SetOptionTable setOptions;
    LevelTable levels;
    SkillTable skills;
    StatusTable statusRates;
    QuestTable quests;
    WarpTable warps;
};
