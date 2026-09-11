#include "CharacterRepository.h"

#include "storage/IDatabase.h"

namespace CharacterRepository
{
std::optional<CoreData> Load(IDatabase& db, std::int64_t characterId)
{
    auto findCharacter =
        db.Prepare("SELECT character.name, character.level, character.job_id, character.gender, "
                   "       character.hairstyle_id, character.face_id, "
                   "       character.stats_str, character.stats_int, character.stats_dex, "
                   "       character.stats_con, character.stats_men, character.stats_sen, "
                   "       character.money, "
                   "       character_position.map_id, character_position.location_x, "
                   "       character_position.location_y, "
                   "       character.unallocated_stat_points, character.exp, "
                   "       character.hp, character.ap, character.fame "
                   "FROM character "
                   "JOIN character_position ON character_position.character_id = character.id "
                   "WHERE character.id = ?");
    findCharacter->Bind(0, characterId);

    if (!findCharacter->Step())
        return std::nullopt;

    CoreData data;

    data.name = std::get<std::string>(findCharacter->Column(0));
    data.level = static_cast<std::int32_t>(std::get<int64_t>(findCharacter->Column(1)));
    data.job_id = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(2)));
    data.gender = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(3)));
    data.hairstyle_id = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(4)));
    data.face_id = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(5)));

    data.raw_stats.strength = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(6)));
    data.raw_stats.intelligence =
        static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(7)));
    data.raw_stats.dexterity = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(8)));
    data.raw_stats.constitution =
        static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(9)));
    data.raw_stats.mentality =
        static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(10)));
    data.raw_stats.sense = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(11)));

    data.money = std::get<int64_t>(findCharacter->Column(12)); // "cegel" on the wire

    data.map_id = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(13)));
    data.x = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(14)));
    data.y = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(15)));

    data.raw_stats.unallocated_stat_points =
        static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(16)));
    data.exp = std::get<int64_t>(findCharacter->Column(17));

    data.hp = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(18)));
    data.ap = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(19)));
    data.fame = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(20)));

    return data;
}

void SavePosition(IDatabase& db, std::int64_t characterId, std::uint32_t mapId, std::uint32_t x,
                   std::uint32_t y)
{
    auto updatePosition = db.Prepare(
        "UPDATE character_position SET map_id = ?, location_x = ?, location_y = ? "
        "WHERE character_id = ?");
    updatePosition->Bind(0, static_cast<int64_t>(mapId));
    updatePosition->Bind(1, static_cast<int64_t>(x));
    updatePosition->Bind(2, static_cast<int64_t>(y));
    updatePosition->Bind(3, characterId);
    updatePosition->Step();
}

std::optional<std::int64_t> TrySpendMoney(IDatabase& db, std::int64_t characterId,
                                           std::int64_t amount)
{
    auto stmt = db.Prepare(
        "UPDATE character SET money = money - ? WHERE id = ? AND money >= ? RETURNING money");
    stmt->Bind(0, amount);
    stmt->Bind(1, characterId);
    stmt->Bind(2, amount);

    if (!stmt->Step())
        return std::nullopt;

    return std::get<int64_t>(stmt->Column(0));
}

std::int64_t AddMoney(IDatabase& db, std::int64_t characterId, std::int64_t amount)
{
    auto stmt =
        db.Prepare("UPDATE character SET money = money + ? WHERE id = ? RETURNING money");
    stmt->Bind(0, amount);
    stmt->Bind(1, characterId);
    stmt->Step();
    return std::get<int64_t>(stmt->Column(0));
}

std::uint32_t AddFame(IDatabase& db, std::int64_t characterId, std::uint32_t amount)
{
    auto stmt =
        db.Prepare("UPDATE character SET fame = fame + ? WHERE id = ? RETURNING fame");
    stmt->Bind(0, static_cast<int64_t>(amount));
    stmt->Bind(1, characterId);
    stmt->Step();
    return static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(0)));
}

std::int64_t AddExp(IDatabase& db, std::int64_t characterId, std::int64_t amount)
{
    auto stmt = db.Prepare("UPDATE character SET exp = exp + ? WHERE id = ? RETURNING exp");
    stmt->Bind(0, amount);
    stmt->Bind(1, characterId);
    stmt->Step();
    return std::get<int64_t>(stmt->Column(0));
}

void SaveVitals(IDatabase& db, std::int64_t characterId, std::uint32_t hp, std::uint32_t ap)
{
    auto updateVitals = db.Prepare("UPDATE character SET hp = ?, ap = ? WHERE id = ?");
    updateVitals->Bind(0, static_cast<int64_t>(hp));
    updateVitals->Bind(1, static_cast<int64_t>(ap));
    updateVitals->Bind(2, characterId);
    updateVitals->Step();
}

void SaveRawStats(IDatabase& db, std::int64_t characterId, const CharacterRawStats& raw)
{
    auto updateStats = db.Prepare(
        "UPDATE character SET stats_str = ?, stats_int = ?, stats_dex = ?, stats_con = ?, "
        "stats_men = ?, stats_sen = ?, unallocated_stat_points = ? WHERE id = ?");
    updateStats->Bind(0, static_cast<int64_t>(raw.strength));
    updateStats->Bind(1, static_cast<int64_t>(raw.intelligence));
    updateStats->Bind(2, static_cast<int64_t>(raw.dexterity));
    updateStats->Bind(3, static_cast<int64_t>(raw.constitution));
    updateStats->Bind(4, static_cast<int64_t>(raw.mentality));
    updateStats->Bind(5, static_cast<int64_t>(raw.sense));
    updateStats->Bind(6, static_cast<int64_t>(raw.unallocated_stat_points));
    updateStats->Bind(7, characterId);
    updateStats->Step();
}

void SaveLevel(IDatabase& db, std::int64_t characterId, std::int32_t level, std::int64_t exp)
{
    auto updateLevel = db.Prepare("UPDATE character SET level = ?, exp = ? WHERE id = ?");
    updateLevel->Bind(0, static_cast<int64_t>(level));
    updateLevel->Bind(1, exp);
    updateLevel->Bind(2, characterId);
    updateLevel->Step();
}
} // namespace CharacterRepository
