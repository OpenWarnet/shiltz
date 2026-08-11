#include "ItemScr.h"

#include <string_view>

namespace
{
    constexpr std::size_t kItemTypeColumn = 2;
    constexpr std::size_t kBuyPriceColumn = 35;
    constexpr std::size_t kSellPriceColumn = 36;

    constexpr std::size_t kDamageScaleColumn = 10;
    constexpr std::size_t kMagicPowerScaleColumn = 16;
    constexpr std::size_t kDefenseScaleColumn = 21;
    constexpr std::size_t kAttackSpeedScaleColumn = 24;
    constexpr std::size_t kAccuracyScaleColumn = 26;
    constexpr std::size_t kCriticalRateScaleColumn = 28;
    constexpr std::size_t kEvasionRateScaleColumn = 30;
    constexpr std::size_t kMovementSpeedScaleColumn = 32;
    constexpr std::size_t kHpPercentScaleColumn = 38;
    constexpr std::size_t kApPercentScaleColumn = 40;

    std::int64_t ColumnOrZero(const std::vector<std::string_view>& row, std::size_t column)
    {
        return row.size() > column ? ScrTable::ParseInt64(row[column]) : 0;
    }

    ItemRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        ItemRecord record;
        record.id = ScrTable::ParseInt64(row[0]);
        record.item_type = ColumnOrZero(row, kItemTypeColumn);
        record.buy_price = ColumnOrZero(row, kBuyPriceColumn);
        record.sell_price = ColumnOrZero(row, kSellPriceColumn);

        record.damage_scale = ColumnOrZero(row, kDamageScaleColumn);
        record.magic_power_scale = ColumnOrZero(row, kMagicPowerScaleColumn);
        record.defense_scale = ColumnOrZero(row, kDefenseScaleColumn);
        record.attack_speed_scale = ColumnOrZero(row, kAttackSpeedScaleColumn);
        record.accuracy_scale = ColumnOrZero(row, kAccuracyScaleColumn);
        record.critical_rate_scale = ColumnOrZero(row, kCriticalRateScaleColumn);
        record.evasion_rate_scale = ColumnOrZero(row, kEvasionRateScaleColumn);
        record.movement_speed_scale = ColumnOrZero(row, kMovementSpeedScaleColumn);
        record.hp_percent_scale = ColumnOrZero(row, kHpPercentScaleColumn);
        record.ap_percent_scale = ColumnOrZero(row, kApPercentScaleColumn);

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
