#pragma once

#include <array>
#include <cstdint>
#include <string>

class PayloadWriter;

struct CharacterQuestFlags
{
    static constexpr std::size_t kWordCount = 128;

    std::array<std::uint32_t, kWordCount> words{};

    void Set(std::uint32_t flagId, bool value = true);
    bool IsSet(std::uint32_t flagId) const;

    void Serialize(PayloadWriter& writer) const;
};

struct CharacterSkillEntry
{
    std::uint16_t skill_id = 0;
    std::uint16_t skill_level = 0;

    void Serialize(PayloadWriter& writer) const;
};

struct CharacterDataLoad
{
    std::uint32_t origin_server_id =
        0; // Unity cross-server origin index (Duran/Arus/... lore-themed server names); 0 on single-server setups
    std::uint32_t self_entity_id = 0;
    std::uint32_t eps_user_flag =
        0; // "EPS USERFLAG" -- Enforced Point System eligibility flag, not a point count
    std::uint32_t map_id = 0;
    std::uint32_t loc_x = 0;
    std::uint32_t loc_y = 0;
    std::uint32_t level = 0;
    std::uint32_t job_id = 0;
    std::uint32_t gender = 0;
    std::int64_t current_exp = 0;
    std::int64_t cegel = 0; // "ownCegel"/"ownFame" -- CIndividualitySystem's Fame value (Individuality System)
    std::uint32_t fame = 0;

    std::uint32_t stats_str = 0;
    std::uint32_t stats_int = 0;
    std::uint32_t stats_dex = 0;
    std::uint32_t stats_con = 0;
    std::uint32_t stats_men = 0;
    std::uint32_t stats_sen = 0;

    std::uint32_t current_hp = 0;
    std::uint32_t current_ap = 0;
    std::uint32_t combat_ranking_points = 0;
    std::uint32_t unused_stat_points = 0;
    std::uint32_t skill_points = 0;
    std::uint32_t enforced_points = 0; // real in-game name: "Enforced Points (EP)"
    std::uint32_t combat_ranking_category = 0;
    std::uint32_t combat_ranking_category_2 = 0;
    std::uint32_t hair_type = 0;
    std::uint32_t polymorph_id = 0;
    std::uint32_t shiltz_time = 0;

    CharacterQuestFlags quest_flags{};
    std::array<CharacterSkillEntry, 64> skill_list{};

    std::string char_name;

    std::uint32_t state_flags =
        0; // packed: low byte = guild_war_entry_state, 0x8000/0x4000 = unidentified flag bits
    std::uint32_t guild_index = 0;
    std::uint32_t channel_id = 0;

    std::array<std::uint8_t, 128>
        record_array_a{}; // 8 x 16-byte record block; purpose not yet identified

    std::uint32_t server_timestamp = 0;
    std::uint32_t costume_slot_index = 0;
    std::uint32_t face_type = 0;
    std::uint32_t pvp_point = 0;
    std::uint32_t f40 = 0; // anti-cheat XOR key seed, not a plain data value
    std::uint32_t f41 = 0; // unidentified
    std::uint32_t minigame_flag = 0;
    std::uint32_t monster_survival_state = 0;
    std::uint32_t monster_survival_state_2 = 0;

    std::array<std::uint8_t, 128> record_array_b{}; // second half of the same 8-slot record block

    std::uint32_t local_user_dir = 0;

    void Serialize(PayloadWriter& writer) const;
};
