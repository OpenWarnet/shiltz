#include "LevelScr.h"

#include <string_view>

namespace
{
    LevelRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        LevelRecord record;
        record.level = ScrTable::ParseInt64(row[0]);

        if (row.size() > 1)
            record.exp = ScrTable::ParseInt64(row[1]);

        if (row.size() > 2)
            record.stat_points_gained = ScrTable::ParseInt64(row[2]);

        if (row.size() > 3)
            record.sp_gained = ScrTable::ParseInt64(row[3]);

        return record;
    }

    void OnLine(std::vector<LevelRecord>& records, size_t lineIndex, const std::vector<std::string_view>& tokens)
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

std::vector<LevelRecord> LevelScr::Load(const std::filesystem::path& path)
{
    std::vector<LevelRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
