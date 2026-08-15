#include "WarpScr.h"

#include <string_view>

namespace
{
    WarpRecord BuildRecord(std::int64_t warpId, const std::vector<std::string_view>& row)
    {
        WarpRecord record;
        record.warp_id = warpId;
        record.server_map_id = ScrTable::ParseInt64(row[0]);

        if (row.size() > 1)
            record.x = ScrTable::ParseInt64(row[1]);
        if (row.size() > 2)
            record.y = ScrTable::ParseInt64(row[2]);

        return record;
    }

    void OnLine(std::vector<WarpRecord>& records, size_t lineIndex, const std::vector<std::string_view>& tokens)
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

        // lineIndex counts the header too, so the first data row (lineIndex
        // 1) is warp_id 0.
        records.push_back(BuildRecord(static_cast<std::int64_t>(lineIndex) - 1, tokens));
    }
} // namespace

std::vector<WarpRecord> WarpScr::Load(const std::filesystem::path& path)
{
    std::vector<WarpRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
