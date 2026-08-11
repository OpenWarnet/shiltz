#include "world/Player.h"

#include "protocol/server/CharacterDataLoad.h"
#include "protocol/server/InventoryItemList.h"
#include "storage/IDatabase.h"

#include <variant>

bool Player::LoadFromDB(IDatabase& db, std::int64_t characterId)
{
    auto findCharacter =
        db.Prepare("SELECT character.name, character.level, character.job_id, character.gender, "
                   "       character.hairstyle_id, character.face_id, "
                   "       character.stats_str, character.stats_int, character.stats_dex, "
                   "       character.stats_con, character.stats_men, character.stats_sen, "
                   "       character.money, "
                   "       character_position.map_id, character_position.location_x, "
                   "       character_position.location_y, "
                   "       character.unallocated_stat_points, character.unallocated_sp, "
                   "       character.unallocated_ep, character.exp, "
                   "       character.hp, character.ap, character.fame "
                   "FROM character "
                   "JOIN character_position ON character_position.character_id = character.id "
                   "WHERE character.id = ?");
    findCharacter->Bind(0, characterId);

    if (!findCharacter->Step())
        return false;

    instance_id = static_cast<std::uint32_t>(characterId);

    name = std::get<std::string>(findCharacter->Column(0));
    level = static_cast<std::int32_t>(std::get<int64_t>(findCharacter->Column(1)));
    job_id = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(2)));
    gender = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(3)));
    hairstyle_id = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(4)));
    face_id = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(5)));

    stats.raw.strength = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(6)));
    stats.raw.intelligence =
        static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(7)));
    stats.raw.dexterity = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(8)));
    stats.raw.constitution =
        static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(9)));
    stats.raw.mentality = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(10)));
    stats.raw.sense = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(11)));

    money = std::get<int64_t>(findCharacter->Column(12)); // "cegel" on the wire, see ToCharacterDataLoad

    map_id = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(13)));
    x = static_cast<std::int32_t>(std::get<int64_t>(findCharacter->Column(14)));
    y = static_cast<std::int32_t>(std::get<int64_t>(findCharacter->Column(15)));

    stats.raw.unallocated_stat_points =
        static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(16)));
    skills.unallocated_sp = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(17)));
    skills.unallocated_ep = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(18)));
    exp = std::get<int64_t>(findCharacter->Column(19));

    hp = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(20)));
    ap = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(21)));
    fame = static_cast<std::uint32_t>(std::get<int64_t>(findCharacter->Column(22)));

    // TODO: xp has no DB column yet -- placeholder. Note this is distinct
    // from `exp` (current_exp on the wire, backing the level.scr curve) --
    // xp isn't read anywhere else in the codebase yet.

    equipment.clear();
    auto findEquipment =
        db.Prepare("SELECT slot, item_id, refine_level FROM equipment_slot WHERE character_id = ?");
    findEquipment->Bind(0, characterId);

    while (findEquipment->Step())
    {
        const SqlValue itemIdColumn = findEquipment->Column(1);
        if (!std::holds_alternative<int64_t>(itemIdColumn))
            continue; // NULL item_id -- empty slot

        const SqlValue refineLevelColumn = findEquipment->Column(2);
        const auto refineLevel = std::holds_alternative<int64_t>(refineLevelColumn)
                                      ? static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn))
                                      : 0;

        equipment.push_back(PlayerEquipmentItem{
            .slot = static_cast<std::uint32_t>(std::get<int64_t>(findEquipment->Column(0))),
            .item_id = static_cast<std::uint32_t>(std::get<int64_t>(itemIdColumn)),
            .refine_level = refineLevel,
        });
    }

    skills.skills.clear();
    auto findSkills =
        db.Prepare("SELECT skill_id, level FROM character_skill WHERE character_id = ?");
    findSkills->Bind(0, characterId);

    while (findSkills->Step())
    {
        skills.skills.push_back(PlayerSkill{
            .id = static_cast<std::uint32_t>(std::get<int64_t>(findSkills->Column(0))),
            .level = static_cast<std::uint32_t>(std::get<int64_t>(findSkills->Column(1))),
        });
    }

    inventory.clear();
    auto findInventory = db.Prepare("SELECT slot_index, item_id, quantity, refine_level FROM "
                                     "inventory_slot WHERE character_id = ?");
    findInventory->Bind(0, characterId);

    while (findInventory->Step())
    {
        const SqlValue quantityColumn = findInventory->Column(2);
        const SqlValue refineLevelColumn = findInventory->Column(3);

        const bool hasRefineLevel = std::holds_alternative<int64_t>(refineLevelColumn);

        inventory.push_back(PlayerInventoryItem{
            .slot_index = static_cast<std::uint32_t>(std::get<int64_t>(findInventory->Column(0))),
            .item_id = static_cast<std::uint32_t>(std::get<int64_t>(findInventory->Column(1))),
            .quantity = std::holds_alternative<int64_t>(quantityColumn)
                            ? static_cast<std::uint32_t>(std::get<int64_t>(quantityColumn))
                            : 0,
            .refine_level = hasRefineLevel
                                ? static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn))
                                : 0,
            .has_refine_level = hasRefineLevel,
        });
    }

    return true;
}

void Player::SaveToDB(IDatabase& db) const
{
    auto updatePosition = db.Prepare(
        "UPDATE character_position SET map_id = ?, location_x = ?, location_y = ? "
        "WHERE character_id = ?");
    updatePosition->Bind(0, static_cast<int64_t>(map_id));
    updatePosition->Bind(1, static_cast<int64_t>(x));
    updatePosition->Bind(2, static_cast<int64_t>(y));
    updatePosition->Bind(3, static_cast<int64_t>(instance_id));
    updatePosition->Step();
}

void Player::SaveMoney(IDatabase& db) const
{
    auto updateMoney = db.Prepare("UPDATE character SET money = ? WHERE id = ?");
    updateMoney->Bind(0, money);
    updateMoney->Bind(1, static_cast<int64_t>(instance_id));
    updateMoney->Step();
}

void Player::SaveFame(IDatabase& db) const
{
    auto updateFame = db.Prepare("UPDATE character SET fame = ? WHERE id = ?");
    updateFame->Bind(0, static_cast<int64_t>(fame));
    updateFame->Bind(1, static_cast<int64_t>(instance_id));
    updateFame->Step();
}

void Player::SaveRawStats(IDatabase& db) const
{
    auto updateStats = db.Prepare(
        "UPDATE character SET stats_str = ?, stats_int = ?, stats_dex = ?, stats_con = ?, "
        "stats_men = ?, stats_sen = ?, unallocated_stat_points = ? WHERE id = ?");
    updateStats->Bind(0, static_cast<int64_t>(stats.raw.strength));
    updateStats->Bind(1, static_cast<int64_t>(stats.raw.intelligence));
    updateStats->Bind(2, static_cast<int64_t>(stats.raw.dexterity));
    updateStats->Bind(3, static_cast<int64_t>(stats.raw.constitution));
    updateStats->Bind(4, static_cast<int64_t>(stats.raw.mentality));
    updateStats->Bind(5, static_cast<int64_t>(stats.raw.sense));
    updateStats->Bind(6, static_cast<int64_t>(stats.raw.unallocated_stat_points));
    updateStats->Bind(7, static_cast<int64_t>(instance_id));
    updateStats->Step();
}

void Player::SaveSkillPoints(IDatabase& db) const
{
    auto updateSkillPoints = db.Prepare(
        "UPDATE character SET unallocated_sp = ?, unallocated_ep = ? WHERE id = ?");
    updateSkillPoints->Bind(0, static_cast<int64_t>(skills.unallocated_sp));
    updateSkillPoints->Bind(1, static_cast<int64_t>(skills.unallocated_ep));
    updateSkillPoints->Bind(2, static_cast<int64_t>(instance_id));
    updateSkillPoints->Step();
}

void Player::SaveSkillLevels(IDatabase& db) const
{
    const auto characterId = static_cast<std::int64_t>(instance_id);

    for (const auto& skill : skills.skills)
    {
        // UPSERT: the target row may not exist yet (character_skill rows
        // aren't pre-seeded per skill) -- same idiom as SaveEquipmentRow.
        auto stmt = db.Prepare(
            "INSERT INTO character_skill (character_id, skill_id, level) VALUES (?, ?, ?) "
            "ON CONFLICT(character_id, skill_id) DO UPDATE SET level = excluded.level");
        stmt->Bind(0, characterId);
        stmt->Bind(1, static_cast<int64_t>(skill.id));
        stmt->Bind(2, static_cast<int64_t>(skill.level));
        stmt->Step();
    }
}

void Player::SaveLevel(IDatabase& db) const
{
    auto updateLevel = db.Prepare("UPDATE character SET level = ?, exp = ? WHERE id = ?");
    updateLevel->Bind(0, static_cast<int64_t>(level));
    updateLevel->Bind(1, exp);
    updateLevel->Bind(2, static_cast<int64_t>(instance_id));
    updateLevel->Step();
}

CharacterDataLoad Player::ToCharacterDataLoad(std::uint32_t epsUserFlag,
                                               std::uint32_t serverTimestamp) const
{
    CharacterDataLoad result{
        .self_entity_id = instance_id,
        .eps_user_flag = epsUserFlag,
        .map_id = map_id,
        .loc_x = static_cast<std::uint32_t>(x),
        .loc_y = static_cast<std::uint32_t>(y),
        .level = static_cast<std::uint32_t>(level),
        .job_id = job_id,
        .gender = gender,
        .current_exp = exp,
        .cegel = money,
        .fame = fame,
        .stats_str = stats.raw.strength,
        .stats_int = stats.raw.intelligence,
        .stats_dex = stats.raw.dexterity,
        .stats_con = stats.raw.constitution,
        .stats_men = stats.raw.mentality,
        .stats_sen = stats.raw.sense,
        .current_hp = hp,
        .current_ap = ap,
        .unused_stat_points = stats.raw.unallocated_stat_points,
        .skill_points = skills.unallocated_sp,
        .enforced_points = skills.unallocated_ep,
        .hair_type = hairstyle_id,
        .char_name = name,
        .server_timestamp = serverTimestamp,
        .face_type = face_id,
    };

    // skill_list is a fixed 64-slot wire array -- silently drop anything
    // past that (no character has come close to 64 learned skills yet, but
    // nothing here enforces it) rather than writing out of bounds.
    for (std::size_t i = 0; i < skills.skills.size() && i < result.skill_list.size(); ++i)
    {
        result.skill_list[i] = CharacterSkillEntry{
            .skill_id = static_cast<std::uint16_t>(skills.skills[i].id),
            .skill_level = static_cast<std::uint16_t>(skills.skills[i].level),
        };
    }

    return result;
}

InventoryItemList Player::ToInventoryItemList() const
{
    InventoryItemList result{.total_count = 0};

    for (const auto& item : equipment)
    {
        if (item.slot >= InventoryItemList::kBagStartSlot)
            continue;

        result.slots[item.slot] = {
            .item_id = item.item_id,
            .qty_or_refine = item.refine_level,
        };
    }

    for (const auto& item : inventory)
    {
        const std::size_t wireSlot = InventoryItemList::kBagStartSlot + item.slot_index;
        if (wireSlot >= InventoryItemList::kTotalSlots)
            continue;

        // refine_level is used as-is; a stackable item's quantity needs
        // -1 since refine=N-1 displays as "N pcs" (see InventoryItemList.h).
        const std::uint32_t qtyOrRefine =
            item.has_refine_level ? item.refine_level
                                   : (item.quantity > 0 ? item.quantity - 1 : 0);

        result.slots[wireSlot] = {
            .item_id = item.item_id,
            .qty_or_refine = qtyOrRefine,
        };
    }

    return result;
}

namespace
{
    enum class SlotKind
    {
        Equipment,
        Inventory,
    };

    struct SlotRef
    {
        SlotKind kind;
        std::int64_t index;
    };

    SlotRef ResolveSlotRef(std::uint32_t wireSlotId)
    {
        if (wireSlotId < InventoryItemList::kBagStartSlot)
            return SlotRef{.kind = SlotKind::Equipment, .index = static_cast<std::int64_t>(wireSlotId)};

        return SlotRef{
            .kind = SlotKind::Inventory,
            .index = static_cast<std::int64_t>(wireSlotId) -
                     static_cast<std::int64_t>(InventoryItemList::kBagStartSlot),
        };
    }

    std::optional<PlayerItemSlot> LoadInventoryRow(IDatabase& db, std::int64_t characterId,
                                                    std::int64_t slotIndex)
    {
        auto stmt = db.Prepare(
            "SELECT item_id, quantity, refine_level, item_level, item_opt2, option_bits, "
            "option_eligible_mask FROM inventory_slot WHERE character_id = ? AND slot_index = ?");
        stmt->Bind(0, characterId);
        stmt->Bind(1, slotIndex);

        if (!stmt->Step())
            return std::nullopt;

        const SqlValue quantityColumn = stmt->Column(1);
        const SqlValue refineLevelColumn = stmt->Column(2);
        const bool hasRefineLevel = std::holds_alternative<int64_t>(refineLevelColumn);

        return PlayerItemSlot{
            .item_id = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(0))),
            .quantity = std::holds_alternative<int64_t>(quantityColumn)
                            ? static_cast<std::uint32_t>(std::get<int64_t>(quantityColumn))
                            : 0,
            .refine_level = hasRefineLevel
                                ? static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn))
                                : 0,
            .has_refine_level = hasRefineLevel,
            .item_level = static_cast<std::int32_t>(std::get<int64_t>(stmt->Column(3))),
            .item_opt2 = static_cast<std::int32_t>(std::get<int64_t>(stmt->Column(4))),
            .option_bits = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(5))),
            .option_eligible_mask = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(6))),
        };
    }

    void SaveInventoryRow(IDatabase& db, std::int64_t characterId, std::int64_t slotIndex,
                          const PlayerItemSlot& content)
    {
        // UPSERT: dest slot_index may have no row yet (content can arrive
        // from equipment_slot, a different table).
        auto stmt = db.Prepare(
            "INSERT INTO inventory_slot (character_id, slot_index, item_id, quantity, refine_level, "
            "item_level, item_opt2, option_bits, option_eligible_mask) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(character_id, slot_index) DO UPDATE SET item_id = excluded.item_id, "
            "quantity = excluded.quantity, refine_level = excluded.refine_level, "
            "item_level = excluded.item_level, item_opt2 = excluded.item_opt2, "
            "option_bits = excluded.option_bits, "
            "option_eligible_mask = excluded.option_eligible_mask");
        stmt->Bind(0, characterId);
        stmt->Bind(1, slotIndex);
        stmt->Bind(2, static_cast<int64_t>(content.item_id));
        // Write the unused side as NULL so has_refine_level round-trips.
        stmt->Bind(3, content.has_refine_level ? SqlValue{} : SqlValue{static_cast<int64_t>(content.quantity)});
        stmt->Bind(4, content.has_refine_level ? SqlValue{static_cast<int64_t>(content.refine_level)} : SqlValue{});
        stmt->Bind(5, static_cast<int64_t>(content.item_level));
        stmt->Bind(6, static_cast<int64_t>(content.item_opt2));
        stmt->Bind(7, static_cast<int64_t>(content.option_bits));
        stmt->Bind(8, static_cast<int64_t>(content.option_eligible_mask));
        stmt->Step();
    }

    void ClearInventoryRow(IDatabase& db, std::int64_t characterId, std::int64_t slotIndex)
    {
        auto stmt = db.Prepare("DELETE FROM inventory_slot WHERE character_id = ? AND slot_index = ?");
        stmt->Bind(0, characterId);
        stmt->Bind(1, slotIndex);
        stmt->Step();
    }

    std::optional<PlayerItemSlot> LoadEquipmentRow(IDatabase& db, std::int64_t characterId,
                                                    std::int64_t slot)
    {
        auto stmt = db.Prepare(
            "SELECT item_id, refine_level, item_level, item_opt2, option_bits, option_eligible_mask "
            "FROM equipment_slot WHERE character_id = ? AND slot = ?");
        stmt->Bind(0, characterId);
        stmt->Bind(1, slot);

        if (!stmt->Step())
            return std::nullopt;

        const SqlValue itemIdColumn = stmt->Column(0);
        if (!std::holds_alternative<int64_t>(itemIdColumn))
            return std::nullopt; // NULL item_id -- explicitly-empty slot row

        const SqlValue refineLevelColumn = stmt->Column(1);
        const bool hasRefineLevel = std::holds_alternative<int64_t>(refineLevelColumn);

        return PlayerItemSlot{
            .item_id = static_cast<std::uint32_t>(std::get<int64_t>(itemIdColumn)),
            .quantity = 0,
            .refine_level = hasRefineLevel
                                ? static_cast<std::uint32_t>(std::get<int64_t>(refineLevelColumn))
                                : 0,
            .has_refine_level = hasRefineLevel,
            .item_level = static_cast<std::int32_t>(std::get<int64_t>(stmt->Column(2))),
            .item_opt2 = static_cast<std::int32_t>(std::get<int64_t>(stmt->Column(3))),
            .option_bits = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(4))),
            .option_eligible_mask = static_cast<std::uint32_t>(std::get<int64_t>(stmt->Column(5))),
        };
    }

    void SaveEquipmentRow(IDatabase& db, std::int64_t characterId, std::int64_t slot,
                          const PlayerItemSlot& content)
    {
        // UPSERT: the target row may not exist yet (equipment_slot rows
        // aren't pre-seeded per slot), or may already exist with a NULL
        // item_id (explicitly-empty convention -- see 0002_add_characters.sql).
        auto stmt = db.Prepare(
            "INSERT INTO equipment_slot (character_id, slot, item_id, refine_level, item_level, "
            "item_opt2, option_bits, option_eligible_mask) VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(character_id, slot) DO UPDATE SET item_id = excluded.item_id, "
            "refine_level = excluded.refine_level, item_level = excluded.item_level, "
            "item_opt2 = excluded.item_opt2, option_bits = excluded.option_bits, "
            "option_eligible_mask = excluded.option_eligible_mask");
        stmt->Bind(0, characterId);
        stmt->Bind(1, slot);
        stmt->Bind(2, static_cast<int64_t>(content.item_id));
        stmt->Bind(3, content.has_refine_level ? SqlValue{static_cast<int64_t>(content.refine_level)} : SqlValue{});
        stmt->Bind(4, static_cast<int64_t>(content.item_level));
        stmt->Bind(5, static_cast<int64_t>(content.item_opt2));
        stmt->Bind(6, static_cast<int64_t>(content.option_bits));
        stmt->Bind(7, static_cast<int64_t>(content.option_eligible_mask));
        stmt->Step();
    }

    void ClearEquipmentRow(IDatabase& db, std::int64_t characterId, std::int64_t slot)
    {
        auto stmt = db.Prepare("UPDATE equipment_slot SET item_id = NULL, refine_level = NULL "
                                "WHERE character_id = ? AND slot = ?");
        stmt->Bind(0, characterId);
        stmt->Bind(1, slot);
        stmt->Step();
    }
} // namespace

std::optional<PlayerItemSlot> Player::LoadItemSlot(IDatabase& db, std::uint32_t wireSlotId) const
{
    const SlotRef ref = ResolveSlotRef(wireSlotId);
    const auto characterId = static_cast<std::int64_t>(instance_id);

    return ref.kind == SlotKind::Equipment ? LoadEquipmentRow(db, characterId, ref.index)
                                            : LoadInventoryRow(db, characterId, ref.index);
}

void Player::SaveItemSlot(IDatabase& db, std::uint32_t wireSlotId, const PlayerItemSlot& content) const
{
    const SlotRef ref = ResolveSlotRef(wireSlotId);
    const auto characterId = static_cast<std::int64_t>(instance_id);

    if (ref.kind == SlotKind::Equipment)
        SaveEquipmentRow(db, characterId, ref.index, content);
    else
        SaveInventoryRow(db, characterId, ref.index, content);
}

void Player::ClearItemSlot(IDatabase& db, std::uint32_t wireSlotId) const
{
    const SlotRef ref = ResolveSlotRef(wireSlotId);
    const auto characterId = static_cast<std::int64_t>(instance_id);

    if (ref.kind == SlotKind::Equipment)
        ClearEquipmentRow(db, characterId, ref.index);
    else
        ClearInventoryRow(db, characterId, ref.index);
}
