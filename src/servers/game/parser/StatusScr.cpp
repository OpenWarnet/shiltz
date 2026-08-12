#include "StatusScr.h"

#include <string_view>

namespace
{
    StatusRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        StatusRecord record;
        record.class_id = ScrTable::ParseInt64(row[0]);

        if (row.size() > 1)
            record.block_id = ScrTable::ParseInt64(row[1]);

        if (row.size() > 2)
            record.value = ScrTable::ParseDouble(row[2]);

        return record;
    }

    void OnLine(std::vector<StatusRecord>& records, size_t lineIndex, const std::vector<std::string_view>& tokens)
    {
        if (lineIndex == 0)
        {
            std::int64_t count = ScrTable::ParseInt64(tokens[0]);
            if (count > 0)
                records.reserve(static_cast<size_t>(count));

            return;
        }

        records.push_back(BuildRecord(tokens));
    }
} // namespace

std::vector<StatusRecord> StatusScr::Load(const std::filesystem::path& path)
{
    std::vector<StatusRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
