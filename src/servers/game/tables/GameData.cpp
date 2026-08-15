#include "GameData.h"

#include <filesystem>

namespace
{
    std::filesystem::path DataDir()
    {
        return std::filesystem::path(SHILTZ_SOURCE_DIR) / "src" / "servers" / "game" / "world" /
               "data";
    }

    std::filesystem::path ItemDataDir()
    {
        return DataDir() / "item";
    }

    std::filesystem::path SkillDataDir()
    {
        return DataDir() / "skill";
    }
} // namespace

void GameData::Load()
{
    monsters.Load(DataDir() / "monster.scr");
    sellers.Load(DataDir() / "seller.scr");
    items.Load(ItemDataDir());
    setOptions.Load(DataDir() / "set_opt.scr");
    levels.Load(DataDir() / "level.scr");
    statusRates.Load(DataDir() / "status.scr");
    skills.Load(SkillDataDir());
    quests.Load(DataDir() / "quest.scr");
    warps.Load(DataDir() / "warp.scr");
}
