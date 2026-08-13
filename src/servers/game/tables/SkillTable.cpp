#include "SkillTable.h"

#include <charconv>
#include <string>
#include <string_view>

namespace
{
    // Packs a (skillId, level) pair into one collision-free int64 key --
    // skillId in the high 32 bits, level in the low 32 bits, so any two
    // distinct pairs always land on distinct keys regardless of either
    // value's magnitude (unlike e.g. skillId * 100 + level, which silently
    // collides if level ever reaches double digits... which it already
    // does, up to skill23.scr).
    std::int64_t MakeKey(std::int64_t skillId, std::int64_t level)
    {
        return (skillId << 32) | (level & 0xFFFFFFFFLL);
    }

    // skillNN.scr's level isn't a column in the row -- it's which file the
    // row came from. Parses "skillNN" -> NN; returns 0 (an invalid level,
    // never matched by MakeKey's callers) for anything that doesn't fit
    // that shape, e.g. a future uskillNN.scr dropped in the same directory.
    std::int64_t ParseFileLevel(const std::filesystem::path& path)
    {
        static constexpr std::string_view kPrefix = "skill";

        const std::string stem = path.stem().string();
        if (stem.size() <= kPrefix.size() || stem.compare(0, kPrefix.size(), kPrefix) != 0)
            return 0;

        std::int64_t level = 0;
        const std::string digits = stem.substr(kPrefix.size());
        auto result = std::from_chars(digits.data(), digits.data() + digits.size(), level);

        return result.ec == std::errc() ? level : 0;
    }
} // namespace

void SkillTable::Load(const std::filesystem::path& dir)
{
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".scr")
            continue;

        const std::int64_t level = ParseFileLevel(entry.path());
        if (level <= 0)
            continue;

        for (auto& record : SkillScr::Load(entry.path()))
        {
            const std::int64_t key = MakeKey(record.id, level);
            m_records.emplace(key, std::move(record));
        }
    }
}

const SkillRecord* SkillTable::Find(std::int64_t skillId, std::int64_t level) const
{
    auto it = m_records.find(MakeKey(skillId, level));
    return it != m_records.end() ? &it->second : nullptr;
}
