#include "MapLoader.h"

#include "Creature.h"
#include "parser/MonsterSpawnScr.h"
#include "parser/NpcScr.h"

Map MapLoader::Load(const std::filesystem::path& npcScrPath,
                     const std::filesystem::path& monsterSpawnScrPath,
                     const std::function<std::uint32_t()>& nextInstanceId)
{
    Map map;

    for (const auto& spawn : NpcScr::Load(npcScrPath))
    {
        for (const auto& instance : spawn.instances)
        {
            map.AddCreature(Creature{
                .instance_id = nextInstanceId(),
                .kind = CreatureKind::Npc,
                .monster_id = spawn.id,
                .x = instance.x,
                .y = instance.y,
                .direction = instance.direction,
            });
        }
    }

    for (const auto& group : MonsterSpawnScr::Load(monsterSpawnScrPath).groups)
    {
        for (const auto& instance : group.instances)
        {
            map.AddCreature(Creature{
                .instance_id = nextInstanceId(),
                .kind = CreatureKind::Monster,
                .monster_id = group.monster_id,
                .x = instance.x,
                .y = instance.y,
                .direction = instance.direction,
            });
        }
    }

    return map;
}
