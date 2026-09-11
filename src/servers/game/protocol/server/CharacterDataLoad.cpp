#include "CharacterDataLoad.h"

#include "common/PayloadWriter.h"

void CharacterQuestFlags::Set(std::uint32_t flagId, bool value)
{
    if (flagId == 0 || flagId > kWordCount * 32)
        return; // flags are 1-based; out of range is a no-op rather than UB

    std::size_t wordIndex = (flagId - 1) / 32;
    std::uint32_t bit = (flagId - 1) % 32;

    if (value)
        words[wordIndex] |= (1u << bit);
    else
        words[wordIndex] &= ~(1u << bit);
}

bool CharacterQuestFlags::IsSet(std::uint32_t flagId) const
{
    if (flagId == 0 || flagId > kWordCount * 32)
        return false;

    std::size_t wordIndex = (flagId - 1) / 32;
    std::uint32_t bit = (flagId - 1) % 32;

    return (words[wordIndex] & (1u << bit)) != 0;
}

void CharacterQuestFlags::Serialize(PayloadWriter& writer) const
{
    writer.Write(words);
}

void CharacterSkillEntry::Serialize(PayloadWriter& writer) const
{
    std::uint32_t packed = (static_cast<std::uint32_t>(skill_id) << 16) | skill_level;
    writer.Write(packed);
}

void CharacterDataLoad::Serialize(PayloadWriter& writer) const
{
    writer.Write(origin_server_id);
    writer.Write(self_entity_id);
    writer.Write(eps_user_flag);
    writer.Write(map_id);
    writer.Write(loc_x);
    writer.Write(loc_y);
    writer.Write(level);
    writer.Write(job_id);
    writer.Write(gender);
    writer.Write(current_exp);
    writer.Write(cegel);
    writer.Write(fame);

    writer.Write(stats_str);
    writer.Write(stats_int);
    writer.Write(stats_dex);
    writer.Write(stats_con);
    writer.Write(stats_men);
    writer.Write(stats_sen);

    writer.Write(current_hp);
    writer.Write(current_ap);
    writer.Write(combat_ranking_points);
    writer.Write(unused_stat_points);
    writer.Write(skill_points);
    writer.Write(enforced_points);
    writer.Write(combat_ranking_category);
    writer.Write(combat_ranking_category_2);
    writer.Write(hair_type);
    writer.Write(polymorph_id);
    writer.Write(shiltz_time);

    quest_flags.Serialize(writer);

    for (const auto& skill : skill_list)
    {
        skill.Serialize(writer);
    }

    writer.WriteString(guild_name, 20);
    writer.Write(guild_state);
    writer.Write(guild_emblem);

    writer.Write(channel_id);

    writer.Write(record_array_a);

    writer.Write(server_timestamp);
    writer.Write(costume_slot_index);
    writer.Write(face_type);
    writer.Write(pvp_point);
    writer.Write(f40);
    writer.Write(f41);
    writer.Write(minigame_flag);

    writer.Write(std::uint32_t{0}); // unread padding dword

    writer.Write(monster_survival_state);
    writer.Write(monster_survival_state_2);

    writer.Write(std::uint32_t{0}); // second unread padding dword

    std::uint8_t reservedGap[68]{};
    writer.Write(reservedGap); // larger unread padding block

    writer.Write(record_array_b);

    writer.Write(local_user_dir);
}
