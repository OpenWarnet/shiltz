#include "NpcScr.h"

#include <string_view>

namespace
{
    NpcSpawn BuildSpawn(const std::vector<std::string_view>& row)
    {
        NpcSpawn spawn;
        spawn.unknown = ScrTable::ParseInt64(row[0]);
        spawn.id = ScrTable::ParseInt64(row[1]);
        std::int64_t count = ScrTable::ParseInt64(row[2]);

        size_t offset = 3;
        for (std::int64_t i = 0; i < count && offset + 3 <= row.size(); ++i, offset += 3)
        {
            NpcInstance instance;
            instance.x = static_cast<std::int32_t>(ScrTable::ParseInt64(row[offset]));
            instance.y = static_cast<std::int32_t>(ScrTable::ParseInt64(row[offset + 1]));
            instance.direction = static_cast<std::int32_t>(ScrTable::ParseInt64(row[offset + 2]));
            spawn.instances.push_back(instance);
        }

        return spawn;
    }

    void OnLine(std::vector<NpcSpawn>& spawns, size_t lineIndex, const std::vector<std::string_view>& tokens)
    {
        if (lineIndex == 1)
        {
            std::int64_t count = ScrTable::ParseInt64(tokens[0]);
            if (count > 0)
            {
                spawns.reserve(static_cast<size_t>(count));
            }

            return;
        }

        if (lineIndex < 2 || tokens.size() < 3)
        {
            return;
        }

        spawns.push_back(BuildSpawn(tokens));
    }
} // namespace

std::vector<NpcSpawn> NpcScr::Load(const std::filesystem::path& path)
{
    std::vector<NpcSpawn> spawns;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(spawns, lineIndex, tokens);
    });
    return spawns;
}
