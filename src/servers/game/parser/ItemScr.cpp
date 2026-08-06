#include "ItemScr.h"

#include <string_view>

namespace
{
    constexpr std::size_t kBuyPriceColumn = 35;
    constexpr std::size_t kSellPriceColumn = 36;

    ItemRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        ItemRecord record;
        record.id = ScrTable::ParseInt64(row[0]);

        if (row.size() > kBuyPriceColumn)
        {
            record.buy_price = ScrTable::ParseInt64(row[kBuyPriceColumn]);
        }

        if (row.size() > kSellPriceColumn)
        {
            record.sell_price = ScrTable::ParseInt64(row[kSellPriceColumn]);
        }

        return record;
    }

    void OnLine(std::vector<ItemRecord>& records, size_t lineIndex, const std::vector<std::string_view>& tokens)
    {
        if (lineIndex == 0)
        {
            std::int64_t count = ScrTable::ParseInt64(tokens[0]);
            if (count > 0)
            {
                records.reserve(records.size() + static_cast<size_t>(count));
            }

            return;
        }

        records.push_back(BuildRecord(tokens));
    }
} // namespace

std::vector<ItemRecord> ItemScr::Load(const std::filesystem::path& path)
{
    std::vector<ItemRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
