#include "MonsterScr.h"

#include <string_view>

namespace
{
    std::int64_t At(const std::vector<std::int64_t>& fields, size_t index)
    {
        return index < fields.size() ? fields[index] : 0;
    }

    MonsterRecord BuildRecord(const std::vector<std::string_view>& row)
    {
        MonsterRecord record;
        record.fields.reserve(row.size());
        for (const auto& token : row)
        {
            record.fields.push_back(ScrTable::ParseInt64(token));
        }

        record.id = At(record.fields, 0);
        record.level = At(record.fields, 2);
        record.hp = At(record.fields, 3);
        record.wander_step_count = At(record.fields, 4);
        record.attack_range = At(record.fields, 5);
        record.element = At(record.fields, 6);
        record.critical_hit_chance = At(record.fields, 7);
        record.critical_hit_defense = At(record.fields, 8);
        record.hit_rate = At(record.fields, 9);
        record.evasion_rate = At(record.fields, 10);
        record.attack = At(record.fields, 11);
        record.defense = At(record.fields, 12);
        record.exp_reward = At(record.fields, 13);
        record.loot_id = At(record.fields, 14);
        record.ai_id = At(record.fields, 15);
        record.category = At(record.fields, 16);
        record.model_id = At(record.fields, 19);
        record.talk_id = At(record.fields, 21);
        record.seller_id = At(record.fields, 22);
        record.pack_flag = At(record.fields, 23);
        record.secondary_ai_id = At(record.fields, 24);
        record.unique_spawn_flag = At(record.fields, 25);
        record.buff_gold_reward = At(record.fields, 26);
        record.respawn_time = At(record.fields, 27);
        record.spawn_scatter_range = At(record.fields, 28);
        record.call_for_help_range = At(record.fields, 29);
        record.link_flag = At(record.fields, 30);

        return record;
    }

    void OnLine(std::vector<MonsterRecord>& records, size_t lineIndex, const std::vector<std::string_view>& tokens)
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

std::vector<MonsterRecord> MonsterScr::Load(const std::filesystem::path& path)
{
    std::vector<MonsterRecord> records;
    ScrTable::Load(path, [&](size_t lineIndex, const std::vector<std::string_view>& tokens) {
        OnLine(records, lineIndex, tokens);
    });
    return records;
}
