#include "MapScr.h"

#include <string_view>

namespace
{
    std::string_view TokenAt(const std::vector<std::string_view>& row, std::size_t index)
    {
        return index < row.size() ? row[index] : std::string_view{};
    }

    // Column indices below are 0-based token positions within a data row
    // (the schema's server_map_id, m_file, and npc_file columns -- m_file
    // is renamed monster_file here since it's the column that actually
    // holds a filename; the schema's own "map_file" column is a decoy that
    // just mirrors the row's ordinal, not a filename).
    MapRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        MapRecord record;
        record.server_map_id = ScrTable::ParseInt64(TokenAt(row, 6));
        record.npc_file = std::string(TokenAt(row, 14));
        record.monster_file = std::string(TokenAt(row, 16));
        return record;
    }

    void OnLine(std::vector<MapRecord>& records, size_t lineIndex, const std::vector<std::string_view>& tokens)
    {
        if (lineIndex == 0)
        {
            std::int64_t count = ScrTable::ParseInt64(tokens[0]);
            if (count > 0)
            {
                records.reserve(static_cast<size_t>(count));
            }

            return;
        }

        records.push_back(BuildRecord(tokens));
    }
} // namespace

std::vector<MapRecord> MapScr::Load(const std::filesystem::path& path)
{
    std::vector<MapRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
