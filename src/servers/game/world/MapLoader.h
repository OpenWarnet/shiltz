#pragma once

#include "Map.h"

#include <cstdint>
#include <filesystem>
#include <functional>

class MonsterTable;

// Builds one Map's creature grid from that map's data files: an npcNN.scr
// (static entities -- dialogue/shop/warp/gacha, see NpcScr.h) and an
// mNN.scr (monster spawns, see MonsterSpawnScr.h). Both feed the same
// Map's creature grid -- NPCs and monsters are combined into one Creature
// list per cell for now (see Creature.h); nothing downstream needs them
// split apart yet.
class MapLoader
{
public:
    // `nextInstanceId` is called once per spawned instance to fill in
    // Creature::instance_id -- callers pass World::AllocateCreatureInstanceId
    // (a std::function rather than a World& so this stays decoupled from
    // World's concrete type). `monsters` fills in each spawned Creature's
    // own MonsterRecord copy (see Creature::monster) -- a missing id just
    // leaves that creature's copy default-constructed, same as
    // MonsterTable::Find's other callers (e.g. Movement.cpp) already
    // treat a miss.
    // Throws std::runtime_error if either file can't be opened.
    static Map Load(const std::filesystem::path& npcScrPath,
                     const std::filesystem::path& monsterSpawnScrPath,
                     const std::function<std::uint32_t()>& nextInstanceId,
                     const MonsterTable& monsters);
};
