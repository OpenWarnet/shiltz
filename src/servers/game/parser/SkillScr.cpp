#include "SkillScr.h"

#include <string_view>

namespace
{
    std::string_view TokenAt(const std::vector<std::string_view>& row, std::size_t index)
    {
        return index < row.size() ? row[index] : std::string_view{};
    }

    SkillRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        SkillRecord record;

        record.id = ScrTable::ParseInt64(TokenAt(row, 0));
        record.name = std::string(TokenAt(row, 1));
        record.job_type = ScrTable::ParseInt64(TokenAt(row, 2));
        record.skill_type = ScrTable::ParseInt64(TokenAt(row, 3));
        record.field_4 = ScrTable::ParseInt64(TokenAt(row, 4));
        record.key = std::string(TokenAt(row, 5));

        record.prereq_skill_id = ScrTable::ParseInt64(TokenAt(row, 6));
        record.prereq_skill_level = ScrTable::ParseInt64(TokenAt(row, 7));
        record.max_skill_level = ScrTable::ParseInt64(TokenAt(row, 8));
        record.skill_points = ScrTable::ParseInt64(TokenAt(row, 9));
        record.min_level = ScrTable::ParseInt64(TokenAt(row, 10));
        record.required_equip_type = ScrTable::ParseInt64(TokenAt(row, 11));
        record.field_10 = ScrTable::ParseInt64(TokenAt(row, 12));
        record.ap_cost = ScrTable::ParseInt64(TokenAt(row, 13));
        record.support_subtype = ScrTable::ParseInt64(TokenAt(row, 14));
        record.cast_num_target = ScrTable::ParseInt64(TokenAt(row, 15));
        record.area_of_effect = ScrTable::ParseInt64(TokenAt(row, 16));
        record.cast_range = ScrTable::ParseInt64(TokenAt(row, 17));

        record.casting_time = ScrTable::ParseDouble(TokenAt(row, 18));
        record.duration_seconds = ScrTable::ParseDouble(TokenAt(row, 19));
        record.cooldown_seconds = ScrTable::ParseDouble(TokenAt(row, 20));

        record.number_of_hits = ScrTable::ParseInt64(TokenAt(row, 21));
        record.damage_pct = ScrTable::ParseInt64(TokenAt(row, 22));
        record.element = ScrTable::ParseInt64(TokenAt(row, 23));
        record.field_23 = ScrTable::ParseInt64(TokenAt(row, 24));

        record.field_24 = ScrTable::ParseInt64(TokenAt(row, 25));
        record.linked_skill_1 = ScrTable::ParseInt64(TokenAt(row, 26));
        record.buff_1_id = ScrTable::ParseInt64(TokenAt(row, 27));
        record.buff_1_duration_seconds = ScrTable::ParseInt64(TokenAt(row, 28));
        record.buff_1_chance = ScrTable::ParseInt64(TokenAt(row, 29));
        record.linked_skill_2 = ScrTable::ParseInt64(TokenAt(row, 30));
        record.buff_2_id = ScrTable::ParseInt64(TokenAt(row, 31));
        record.buff_2_duration_seconds = ScrTable::ParseInt64(TokenAt(row, 32));
        record.buff_2_chance = ScrTable::ParseInt64(TokenAt(row, 33));
        record.buff_3_id = ScrTable::ParseInt64(TokenAt(row, 34));
        record.buff_4_id = ScrTable::ParseInt64(TokenAt(row, 35));
        record.field_35 = ScrTable::ParseInt64(TokenAt(row, 36));
        record.field_36 = ScrTable::ParseInt64(TokenAt(row, 37));

        record.icon_id = ScrTable::ParseInt64(TokenAt(row, 38));
        record.projectile_speed = ScrTable::ParseInt64(TokenAt(row, 39));
        record.field_39 = ScrTable::ParseInt64(TokenAt(row, 40));

        record.field_40 = ScrTable::ParseInt64(TokenAt(row, 41));

        record.ultimate_move_pct = ScrTable::ParseDouble(TokenAt(row, 42));
        record.field_42 = ScrTable::ParseDouble(TokenAt(row, 43));

        record.field_43 = ScrTable::ParseInt64(TokenAt(row, 44));
        record.description = std::string(TokenAt(row, 45));

        return record;
    }

    void OnLine(std::vector<SkillRecord>& records, size_t lineIndex, const std::vector<std::string_view>& tokens)
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

std::vector<SkillRecord> SkillScr::Load(const std::filesystem::path& path)
{
    std::vector<SkillRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
