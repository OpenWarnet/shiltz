#include "SetOptScr.h"

#include <string_view>

namespace
{
    std::int64_t At(const std::vector<std::int64_t>& fields, size_t index)
    {
        return index < fields.size() ? fields[index] : 0;
    }

    SetOptionRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        std::vector<std::int64_t> fields;
        fields.reserve(row.size());
        for (const auto& token : row)
        {
            fields.push_back(ScrTable::ParseInt64(token));
        }

        SetOptionRecord record;
        record.set_id = At(fields, 0);
        record.piece_count = At(fields, 1);

        record.damage = At(fields, 2);
        record.magic = At(fields, 3);
        record.defense = At(fields, 4);
        record.attack_speed = At(fields, 5);
        record.accuracy = At(fields, 6);
        record.critical_rate = At(fields, 7);
        record.evasion = At(fields, 8);
        record.movement_speed = At(fields, 9);
        record.hp_percent = At(fields, 10);
        record.ap_percent = At(fields, 11);

        return record;
    }

    void OnLine(std::vector<SetOptionRecord>& records, size_t lineIndex,
                const std::vector<std::string_view>& tokens)
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

std::vector<SetOptionRecord> SetOptScr::Load(const std::filesystem::path& path)
{
    std::vector<SetOptionRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
