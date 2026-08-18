#include "AiMonScr.h"

#include <string_view>

namespace
{
    AiMonRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        AiMonRecord record;
        record.id = ScrTable::ParseInt64(row[0]);

        if (row.size() > 1)
        {
            record.unknown_1 = ScrTable::ParseInt64(row[1]);
        }

        if (row.size() > 2)
        {
            record.field_1 = ScrTable::ParseInt64(row[2]);
        }

        if (row.size() > 3)
        {
            record.pass_chance = ScrTable::ParseInt64(row[3]);
        }

        std::size_t column = 4;
        for (std::size_t i = 0; i < AiMonRecord::kSkillPickCount && column + 1 < row.size(); ++i)
        {
            record.skill_picks[i].skill_type = ScrTable::ParseInt64(row[column]);
            record.skill_picks[i].weight = ScrTable::ParseInt64(row[column + 1]);
            column += 2;
        }

        for (std::size_t i = 0; i < AiMonRecord::kTriggerCount && column + 5 < row.size(); ++i)
        {
            AiMonRecord::Trigger& trigger = record.triggers[i];
            trigger.trigger_type = ScrTable::ParseInt64(row[column]);
            trigger.parameter = ScrTable::ParseInt64(row[column + 1]);
            trigger.cooldown_ms = ScrTable::ParseInt64(row[column + 2]);
            trigger.max_uses = ScrTable::ParseInt64(row[column + 3]);
            trigger.fire_chance = ScrTable::ParseInt64(row[column + 4]);
            trigger.skill_type = ScrTable::ParseInt64(row[column + 5]);
            column += 6;
        }

        return record;
    }

    void OnLine(std::vector<AiMonRecord>& records, size_t lineIndex, const std::vector<std::string_view>& tokens)
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

std::vector<AiMonRecord> AiMonScr::Load(const std::filesystem::path& path)
{
    std::vector<AiMonRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
