#include "Character.h"

#include "LoginOpcodes.h"
#include "LoginPacket.h"
#include "LoginSessionStore.h"
#include "common/PayloadWriter.h"
#include "common/TCPServer.h"
#include "protocol/client/CreateCharacter.h"
#include "protocol/client/ServerSelect.h"
#include "protocol/client/SetCharacterMap.h"
#include "protocol/server/CharacterSelection.h"
#include "protocol/server/CreateCharacterFail.h"
#include "protocol/shared/GenericCharacterPayload.h"
#include "storage/IDatabase.h"
#include "storage/Transaction.h"

#include <ctime>
#include <iostream>
#include <utility>
#include <vector>

namespace
{
    // TODO: Move these constants to a shared header so that the client and server can use the same
    // values.
    constexpr int64_t kEquipmentSlotHeadgear = 0;
    constexpr int64_t kEquipmentSlotTop = 1;
    constexpr int64_t kEquipmentSlotBottom = 2;
    constexpr int64_t kEquipmentSlotShoes = 3;
    constexpr int64_t kEquipmentSlotWeapon = 4;
    constexpr int64_t kEquipmentSlotShield = 5;
    constexpr int64_t kEquipmentSlotAccessory = 6;
} // namespace

void HandleGetCharacterList(const LoginContext& ctx, const ServerSelect& select)
{
    auto accountId = ctx.sessions.GetAccountId(ctx.clientSocket);
    if (!accountId)
    {
        std::cout << "Rejecting CL_GET_CHARINFO: socket has no authenticated account\n";
        return;
    }

    const int64_t now = static_cast<int64_t>(std::time(nullptr));

    // Sweep characters whose deletion grace period has elapsed before listing
    // -- three bulk statements scoped to (account_id, server_id), regardless
    // of how many characters are actually expired, rather than checking/
    // deleting per-row in the loop below. Children first (unenforced FKs --
    // see 0002_add_characters.sql -- so nothing stops us leaving them behind
    // if we did the parent first).
    const char* kExpiredWhere =
        "account_id = ? AND server_id = ? AND scheduled_deletion_at > 0 AND scheduled_deletion_at < ?";

    {
        // All three DELETEs must land together, or a crash between them
        // leaves an orphaned `character` row.
        DatabaseTransaction sweepTxn(ctx.db);

        auto deleteExpiredEquipment = ctx.db.Prepare(
            std::string(
                "DELETE FROM equipment_slot WHERE character_id IN (SELECT id FROM character WHERE ") +
            kExpiredWhere + ")");
        deleteExpiredEquipment->Bind(0, *accountId);
        deleteExpiredEquipment->Bind(1, static_cast<int64_t>(select.server_id));
        deleteExpiredEquipment->Bind(2, now);
        deleteExpiredEquipment->Step();

        auto deleteExpiredPositions = ctx.db.Prepare(
            std::string(
                "DELETE FROM character_position WHERE character_id IN (SELECT id FROM character WHERE ") +
            kExpiredWhere + ")");
        deleteExpiredPositions->Bind(0, *accountId);
        deleteExpiredPositions->Bind(1, static_cast<int64_t>(select.server_id));
        deleteExpiredPositions->Bind(2, now);
        deleteExpiredPositions->Step();

        auto deleteExpiredCharacters =
            ctx.db.Prepare(std::string("DELETE FROM character WHERE ") + kExpiredWhere);
        deleteExpiredCharacters->Bind(0, *accountId);
        deleteExpiredCharacters->Bind(1, static_cast<int64_t>(select.server_id));
        deleteExpiredCharacters->Bind(2, now);
        deleteExpiredCharacters->Step();

        sweepTxn.Commit();
    }

    std::vector<Character> characters;

    auto findCharacters = ctx.db.Prepare(
        "SELECT id, name, slot, level, job_id, gender, hairstyle_id, face_id, scheduled_deletion_at "
        "FROM character WHERE account_id = ? AND server_id = ? ORDER BY slot");
    findCharacters->Bind(0, *accountId);
    findCharacters->Bind(1, static_cast<int64_t>(select.server_id));

    while (findCharacters->Step())
    {
        const int64_t characterId = std::get<int64_t>(findCharacters->Column(0));

        Character character{
            .name = std::get<std::string>(findCharacters->Column(1)),
            .slot = static_cast<uint32_t>(std::get<int64_t>(findCharacters->Column(2))),
            .level = static_cast<uint32_t>(std::get<int64_t>(findCharacters->Column(3))),
            .job = static_cast<uint32_t>(std::get<int64_t>(findCharacters->Column(4))),
        };
        character.appearance.gender = static_cast<uint32_t>(std::get<int64_t>(findCharacters->Column(5)));
        character.appearance.hairStyle =
            static_cast<uint32_t>(std::get<int64_t>(findCharacters->Column(6)));
        character.appearance.faceStyle =
            static_cast<uint32_t>(std::get<int64_t>(findCharacters->Column(7)));

        const int64_t scheduledDeletionAt = std::get<int64_t>(findCharacters->Column(8));
        if (scheduledDeletionAt > 0)
        {
            character.appearance.isDeleted = true;
            character.appearance.deletionInSeconds =
                static_cast<uint32_t>(scheduledDeletionAt > now ? scheduledDeletionAt - now : 0);
        }

        auto findEquipment = ctx.db.Prepare(
            "SELECT slot, item_id, refine_level FROM equipment_slot WHERE character_id = ?");
        findEquipment->Bind(0, characterId);

        while (findEquipment->Step())
        {
            const int64_t slot = std::get<int64_t>(findEquipment->Column(0));
            const SqlValue itemIdColumn = findEquipment->Column(1);
            const SqlValue refineLevelColumn = findEquipment->Column(2);

            const uint32_t itemId = std::holds_alternative<int64_t>(itemIdColumn)
                                         ? static_cast<uint32_t>(std::get<int64_t>(itemIdColumn))
                                         : 0;
            const uint32_t refineLevel =
                std::holds_alternative<int64_t>(refineLevelColumn)
                    ? static_cast<uint32_t>(std::get<int64_t>(refineLevelColumn))
                    : 0;

            switch (slot)
            {
            case kEquipmentSlotHeadgear:
                character.appearance.headgear = itemId;
                character.appearance.headgearRefine = refineLevel;
                break;
            case kEquipmentSlotTop:
                character.appearance.top = itemId;
                character.appearance.topRefine = refineLevel;
                break;
            case kEquipmentSlotBottom:
                character.appearance.bottom = itemId;
                character.appearance.bottomRefine = refineLevel;
                break;
            case kEquipmentSlotShoes:
                character.appearance.shoes = itemId;
                character.appearance.shoesRefine = refineLevel;
                break;
            case kEquipmentSlotWeapon:
                character.appearance.weapon = itemId;
                character.appearance.weaponRefine = refineLevel;
                break;
            case kEquipmentSlotShield:
                character.appearance.shield = itemId;
                character.appearance.shieldRefine = refineLevel;
                break;
            case kEquipmentSlotAccessory:
                character.appearance.accessory = itemId;
                character.appearance.accessoryRefine = refineLevel;
                break;
            default:
                break;
            }
        }

        characters.push_back(std::move(character));
    }

    PayloadWriter writer;
    CharacterSelection selection{
        .server_id = select.server_id,
        .char_slot_count = 5,
        .characters = std::move(characters),
    };
    selection.Serialize(writer);
    auto charData = writer.Data();

    LoginPacket responsePacket(LoginOpcode::LC_CHARINFO_SUCCESS, charData);
    auto response = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, response);
}

void HandleDeleteCharacter(const LoginContext& ctx, const GenericCharacterPayload& request)
{
    auto sendFail = [&]
    {
        PayloadWriter failWriter;
        GenericCharacterPayload failResponse{.server_id = request.server_id,
                                             .char_name = request.char_name};
        failResponse.Serialize(failWriter);
        auto failData = failWriter.Data();

        LoginPacket failPacket(LoginOpcode::LC_DELETECHAR_FAIL, failData);
        auto failPayload = failPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, failPayload);
    };

    auto accountId = ctx.sessions.GetAccountId(ctx.clientSocket);
    if (!accountId)
    {
        std::cout << "Rejecting CL_DELETE_CHARACTER: socket has no authenticated account\n";
        sendFail();
        return;
    }

    auto findCharacter =
        ctx.db.Prepare("SELECT id FROM character WHERE account_id = ? AND server_id = ? AND name = ?");
    findCharacter->Bind(0, *accountId);
    findCharacter->Bind(1, static_cast<int64_t>(request.server_id));
    findCharacter->Bind(2, request.char_name);

    if (!findCharacter->Step())
    {
        std::cout << "Rejecting CL_DELETE_CHARACTER: no character '" << request.char_name
                  << "' on server " << request.server_id << " for this account\n";
        sendFail();
        return;
    }

    constexpr int64_t kSevenDaysInSeconds = 7 * 24 * 60 * 60;
    const int64_t scheduledDeletionAt = static_cast<int64_t>(std::time(nullptr)) + kSevenDaysInSeconds;

    auto updateCharacter = ctx.db.Prepare("UPDATE character SET scheduled_deletion_at = ? WHERE id = ?");
    updateCharacter->Bind(0, scheduledDeletionAt);
    updateCharacter->Bind(1, std::get<int64_t>(findCharacter->Column(0)));
    updateCharacter->Step();

    PayloadWriter writer;
    GenericCharacterPayload response{.server_id = request.server_id,
                                     .char_name = request.char_name};
    response.Serialize(writer);
    auto data = writer.Data();

    LoginPacket responsePacket(LoginOpcode::LC_DELETECHAR_SUCCESS, data);
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);
}

void HandleCancelDeleteCharacter(const LoginContext& ctx, const GenericCharacterPayload& request)
{
    auto sendFail = [&]
    {
        PayloadWriter failWriter;
        GenericCharacterPayload failResponse{.server_id = request.server_id,
                                             .char_name = request.char_name};
        failResponse.Serialize(failWriter);
        auto failData = failWriter.Data();

        LoginPacket failPacket(LoginOpcode::LC_CHAR_DELETE_CANCLE_FAIL, failData);
        auto failPayload = failPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, failPayload);
    };

    auto accountId = ctx.sessions.GetAccountId(ctx.clientSocket);
    if (!accountId)
    {
        std::cout << "Rejecting CL_CHAR_DELETE_CANCLE: socket has no authenticated account\n";
        sendFail();
        return;
    }

    auto findCharacter =
        ctx.db.Prepare("SELECT id FROM character WHERE account_id = ? AND server_id = ? AND name = ?");
    findCharacter->Bind(0, *accountId);
    findCharacter->Bind(1, static_cast<int64_t>(request.server_id));
    findCharacter->Bind(2, request.char_name);

    if (!findCharacter->Step())
    {
        std::cout << "Rejecting CL_CHAR_DELETE_CANCLE: no character '" << request.char_name
                  << "' on server " << request.server_id << " for this account\n";
        sendFail();
        return;
    }

    auto updateCharacter = ctx.db.Prepare("UPDATE character SET scheduled_deletion_at = 0 WHERE id = ?");
    updateCharacter->Bind(0, std::get<int64_t>(findCharacter->Column(0)));
    updateCharacter->Step();

    PayloadWriter writer;
    GenericCharacterPayload response{.server_id = request.server_id,
                                     .char_name = request.char_name};
    response.Serialize(writer);
    auto data = writer.Data();

    LoginPacket responsePacket(LoginOpcode::LC_CHAR_DELETE_CANCLE_SUCCESS, data);
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);
}

void HandleCreateCharacter(const LoginContext& ctx, const CreateCharacter& request)
{
    auto sendFail = [&]
    {
        PayloadWriter failWriter;
        CreateCharacterFail{}.Serialize(failWriter);
        auto failData = failWriter.Data();

        LoginPacket failPacket(LoginOpcode::LC_CREATECHAR_FAIL, failData);
        auto failPayload = failPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, failPayload);
    };

    if (request.char_name.size() < 4 || request.char_name.size() > 16)
    {
        std::cout << "Rejecting CL_CREATE_CHARACTER: name '" << request.char_name
                  << "' must be 4-16 characters\n";
        sendFail();
        return;
    }

    auto accountId = ctx.sessions.GetAccountId(ctx.clientSocket);
    if (!accountId)
    {
        std::cout << "Rejecting CL_CREATE_CHARACTER: socket has no authenticated account\n";
        sendFail();
        return;
    }

    auto findExisting = ctx.db.Prepare("SELECT 1 FROM character WHERE server_id = ? AND name = ?");
    findExisting->Bind(0, static_cast<int64_t>(request.server_id));
    findExisting->Bind(1, request.char_name);

    if (findExisting->Step())
    {
        std::cout << "Rejecting CL_CREATE_CHARACTER: name '" << request.char_name
                  << "' already taken on server " << request.server_id << "\n";
        sendFail();
        return;
    }

    // Prior hardcoded placeholder, now the actual starting balance (see
    // 0005_add_character_money.sql).
    constexpr int64_t kStartingMoney = 100000;

    // A crash between the two INSERTs below would leave a `character` row
    // with no matching `character_position` row, which LoadFromDB's JOIN
    // would then never find -- silently soft-locking the character.
    DatabaseTransaction createTxn(ctx.db);

    auto insertCharacter = ctx.db.Prepare(
        "INSERT INTO character "
        "(account_id, server_id, slot, name, gender, hairstyle_id, face_id, job_id, level, "
        " stats_str, stats_int, stats_dex, stats_con, stats_men, stats_sen, money) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insertCharacter->Bind(0, *accountId);
    insertCharacter->Bind(1, static_cast<int64_t>(request.server_id));
    insertCharacter->Bind(2, static_cast<int64_t>(request.slot));
    insertCharacter->Bind(3, request.char_name);
    insertCharacter->Bind(4, static_cast<int64_t>(request.gender));
    insertCharacter->Bind(5, static_cast<int64_t>(request.hairstyle));
    insertCharacter->Bind(6, static_cast<int64_t>(0));
    insertCharacter->Bind(7, static_cast<int64_t>(request.job));
    insertCharacter->Bind(8, static_cast<int64_t>(1)); // level 1 at creation
    insertCharacter->Bind(9, static_cast<int64_t>(request.stat_str));
    insertCharacter->Bind(10, static_cast<int64_t>(request.stat_int));
    insertCharacter->Bind(11, static_cast<int64_t>(request.stat_dex));
    insertCharacter->Bind(12, static_cast<int64_t>(request.stat_con));
    insertCharacter->Bind(13, static_cast<int64_t>(request.stat_men));
    insertCharacter->Bind(14, static_cast<int64_t>(request.stat_sen));
    insertCharacter->Bind(15, kStartingMoney);
    insertCharacter->Step();

    const int64_t characterId = ctx.db.LastInsertRowId();

    auto insertPosition = ctx.db.Prepare(
        "INSERT INTO character_position (character_id, map_id, location_x, location_y) "
        "VALUES (?, ?, ?, ?)");
    insertPosition->Bind(0, characterId);
    insertPosition->Bind(1, static_cast<int64_t>(request.map_id));
    insertPosition->Bind(2, static_cast<int64_t>(request.loc_x));
    insertPosition->Bind(3, static_cast<int64_t>(request.loc_y));
    insertPosition->Step();

    createTxn.Commit();

    PayloadWriter writer;
    GenericCharacterPayload response{.server_id = request.server_id,
                                     .char_name = request.char_name};
    response.Serialize(writer);
    auto data = writer.Data();

    LoginPacket responsePacket(LoginOpcode::LC_CREATECHAR_SUCCESS, data);
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);
}

void HandleUpdateCharacterLocation(const LoginContext& ctx, const SetCharacterMap& request)
{
    auto sendFail = [&]
    {
        PayloadWriter failWriter;
        GenericCharacterPayload failResponse{.server_id = request.server_id,
                                             .char_name = request.char_name};
        failResponse.Serialize(failWriter);
        auto failData = failWriter.Data();

        LoginPacket failPacket(LoginOpcode::LC_CREATE_MAP_NUM_FAIL, failData);
        auto failPayload = failPacket.Serialize(ctx.key);

        ctx.server.SendTo(ctx.clientSocket, failPayload);
    };

    auto accountId = ctx.sessions.GetAccountId(ctx.clientSocket);
    if (!accountId)
    {
        std::cout << "Rejecting CL_CREATE_MAP_NUM: socket has no authenticated account\n";
        sendFail();
        return;
    }

    auto findCharacter =
        ctx.db.Prepare("SELECT id FROM character WHERE account_id = ? AND server_id = ? AND name = ?");
    findCharacter->Bind(0, *accountId);
    findCharacter->Bind(1, static_cast<int64_t>(request.server_id));
    findCharacter->Bind(2, request.char_name);

    if (!findCharacter->Step())
    {
        std::cout << "Rejecting CL_CREATE_MAP_NUM: no character '" << request.char_name
                  << "' on server " << request.server_id << " for this account\n";
        sendFail();
        return;
    }

    const int64_t characterId = std::get<int64_t>(findCharacter->Column(0));

    auto updatePosition = ctx.db.Prepare(
        "UPDATE character_position SET map_id = ?, location_x = ?, location_y = ? "
        "WHERE character_id = ?");
    updatePosition->Bind(0, static_cast<int64_t>(request.map_id));
    updatePosition->Bind(1, static_cast<int64_t>(request.loc_x));
    updatePosition->Bind(2, static_cast<int64_t>(request.loc_y));
    updatePosition->Bind(3, characterId);
    updatePosition->Step();

    PayloadWriter writer;
    GenericCharacterPayload response{.server_id = request.server_id,
                                     .char_name = request.char_name};
    response.Serialize(writer);
    auto data = writer.Data();

    LoginPacket responsePacket(LoginOpcode::LC_CREATE_MAP_NUM_SUCCESS, data);
    auto responsePayload = responsePacket.Serialize(ctx.key);

    ctx.server.SendTo(ctx.clientSocket, responsePayload);
}
