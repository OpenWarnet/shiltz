#include "SellerScr.h"

#include <string_view>

namespace
{
    SellerRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        SellerRecord record;
        record.shop_id = ScrTable::ParseInt64(row[0]);

        for (std::size_t i = 0; i < SellerRecord::kItemCount && i + 1 < row.size(); ++i)
        {
            record.items[i] = ScrTable::ParseInt64(row[i + 1]);
        }

        return record;
    }

    void OnLine(std::vector<SellerRecord>& records, size_t lineIndex, const std::vector<std::string_view>& tokens)
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

std::vector<SellerRecord> SellerScr::Load(const std::filesystem::path& path)
{
    std::vector<SellerRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
