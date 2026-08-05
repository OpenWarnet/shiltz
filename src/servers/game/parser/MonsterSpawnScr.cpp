#include "MonsterSpawnScr.h"

#include <string_view>

namespace
{
    MonsterSpawnGroup BuildGroup(const std::vector<std::string_view>& row, bool hasDirection)
    {
        MonsterSpawnGroup group;
        group.monster_id = ScrTable::ParseInt64(row[0]);
        std::int64_t count = ScrTable::ParseInt64(row[1]);

        size_t stride = hasDirection ? 3 : 2;
        size_t offset = 2;
        for (std::int64_t i = 0; i < count && offset + stride <= row.size(); ++i, offset += stride)
        {
            MonsterSpawnInstance instance;
            instance.x = static_cast<std::int32_t>(ScrTable::ParseInt64(row[offset]));
            instance.y = static_cast<std::int32_t>(ScrTable::ParseInt64(row[offset + 1]));
            if (hasDirection)
            {
                instance.direction = static_cast<std::int32_t>(ScrTable::ParseInt64(row[offset + 2]));
            }

            group.instances.push_back(instance);
        }

        return group;
    }

    void OnLine(MonsterSpawnTable& table, size_t lineIndex, const std::vector<std::string_view>& tokens)
    {
        if (lineIndex == 0)
        {
            table.has_direction = ScrTable::ParseInt64(tokens[0]) != 0;
            return;
        }

        if (lineIndex == 1)
        {
            return;
        }

        if (lineIndex == 2)
        {
            std::int64_t count = ScrTable::ParseInt64(tokens[0]);
            if (count > 0)
            {
                table.groups.reserve(static_cast<size_t>(count));
            }

            return;
        }

        if (tokens.size() < 2)
        {
            return;
        }

        table.groups.push_back(BuildGroup(tokens, table.has_direction));
    }
} // namespace

MonsterSpawnTable MonsterSpawnScr::Load(const std::filesystem::path& path)
{
    MonsterSpawnTable table;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(table, lineIndex, tokens);
    });
    return table;
}
