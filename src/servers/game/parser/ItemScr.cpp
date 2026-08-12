#include "ItemScr.h"

#include <string_view>

namespace
{
    constexpr std::size_t kItemTypeColumn = 2;
    constexpr std::size_t kBuyPriceColumn = 35;
    constexpr std::size_t kSellPriceColumn = 36;

    constexpr std::size_t kDamageBonusColumn = 6;
    constexpr std::size_t kDamageDealtIncreasePercentBonusColumn = 9;
    constexpr std::size_t kMagicPowerBonusColumn = 12;
    constexpr std::size_t kDefenseBonusColumn = 17;
    constexpr std::size_t kDamageTakenDecreasePercentBonusColumn = 20;
    constexpr std::size_t kAttackSpeedBonusColumn = 23;
    constexpr std::size_t kAccuracyBonusColumn = 25;
    constexpr std::size_t kCriticalRateBonusColumn = 27;
    constexpr std::size_t kEvasionRateBonusColumn = 29;
    constexpr std::size_t kMovementSpeedBonusColumn = 31;
    constexpr std::size_t kHpBonusColumn = 37;
    constexpr std::size_t kApBonusColumn = 39;

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

    constexpr std::size_t kSetIdColumn = 33;

    constexpr std::size_t kRefineDamageScaleColumn = 8;
    constexpr std::size_t kRefineMagicScaleColumn = 14;
    constexpr std::size_t kRefineDefenseScaleColumn = 19;
    constexpr std::size_t kRefineGroupColumn = 46;

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

        record.damage_bonus = ColumnOrZero(row, kDamageBonusColumn);
        record.damage_dealt_increase_percent_bonus =
            ColumnOrZero(row, kDamageDealtIncreasePercentBonusColumn);
        record.magic_power_bonus = ColumnOrZero(row, kMagicPowerBonusColumn);
        record.defense_bonus = ColumnOrZero(row, kDefenseBonusColumn);
        record.damage_taken_decrease_percent_bonus =
            ColumnOrZero(row, kDamageTakenDecreasePercentBonusColumn);
        record.attack_speed_bonus = ColumnOrZero(row, kAttackSpeedBonusColumn);
        record.accuracy_bonus = ColumnOrZero(row, kAccuracyBonusColumn);
        record.critical_rate_bonus = ColumnOrZero(row, kCriticalRateBonusColumn);
        record.evasion_rate_bonus = ColumnOrZero(row, kEvasionRateBonusColumn);
        record.movement_speed_bonus = ColumnOrZero(row, kMovementSpeedBonusColumn);
        record.hp_bonus = ColumnOrZero(row, kHpBonusColumn);
        record.ap_bonus = ColumnOrZero(row, kApBonusColumn);

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

        record.set_id = ColumnOrZero(row, kSetIdColumn);

        record.refine_damage_scale = ColumnOrZero(row, kRefineDamageScaleColumn);
        record.refine_magic_scale = ColumnOrZero(row, kRefineMagicScaleColumn);
        record.refine_defense_scale = ColumnOrZero(row, kRefineDefenseScaleColumn);
        record.refine_group = ColumnOrZero(row, kRefineGroupColumn);

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
