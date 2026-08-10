#pragma once

#include "world/Player.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <winsock2.h>

// A resolved game session: which account/character owns this connection.
// account_id is looked up from the `session` table (see LoginSessionStore --
// the login server writes a row there on a successful CL_LOGIN, keyed by
// the session_id it later hands the client via GameConnectSuccess);
// character_id is resolved from `character` (account_id, name) once
// CG_ENTER's char_name is known. Set by HandleEnter once both are
// resolved; consulted by any later handler on the same connection that
// needs to know the owning account/character (e.g. CG_ITEM_PICKUP scoping
// an inventory_slot write to "this connection's character"). `player`
// carries this connection's live position/view state, updated by
// HandleEnter (initial position) and HandleMovement (every CG_MOVE).
struct GameSession
{
    std::int64_t sessionId = 0;
    std::int64_t accountId = 0;
    std::int64_t characterId = 0;
    Player player;
};

// Maps a connected socket to its resolved GameSession. Threaded through
// GameContext the same way IDatabase is -- GameServer owns one instance
// and hands out a reference per dispatched frame.
//
// Also enforces at most one active connection per character (see
// TryClaimCharacter) -- otherwise two connections could hold independent
// Player copies mutating the same DB rows concurrently.
class GameSessionStore
{
public:
    void Set(SOCKET clientSocket, GameSession session);
    std::optional<GameSession> Get(SOCKET clientSocket) const;

    // True if characterId was free or already claimed by this socket.
    // False if another socket holds it -- caller must reject.
    bool TryClaimCharacter(std::int64_t characterId, SOCKET clientSocket);

    // Undoes a claim made without a following Set() (e.g. LoadFromDB failed).
    void ReleaseCharacterClaim(std::int64_t characterId);

    // Erases clientSocket's session and its character claim. Call on
    // disconnect and on CG_EXIT.
    void Remove(SOCKET clientSocket);

private:
    mutable std::mutex m_mutex;
    std::unordered_map<SOCKET, GameSession> m_sessionsBySocket;
    std::unordered_map<std::int64_t, SOCKET> m_socketByCharacterId;
};
