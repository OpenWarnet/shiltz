#pragma once

#include "protocol/server/CharacterDataLoad.h" // CharacterQuestFlags

#include <cstdint>

class IDatabase;

// Owns every DB access for the quest_flags table -- one row per
// (character_id, flag id) set so far. Backs both has_flag (CONDITIONS) and
// set_flag (CONSEQUENCES) in quest.scr -- see handlers/Quest.cpp -- and the
// quest_flags bitset sent to the client on CG_ENTER (protocol/server/
// CharacterDataLoad.h).
namespace QuestFlagRepository
{
    // Full load, for Player::LoadFromDB -- returns every flag this
    // character has ever had set, packed into the same bitset shape the
    // wire and the CONDITIONS/CONSEQUENCES checks both use.
    CharacterQuestFlags LoadAll(IDatabase& db, std::int64_t characterId);

    // Persists a single flag as set. quest_flags has no "unset" path in
    // quest.scr's data (set_flag only ever turns a flag on), so there's no
    // corresponding Clear().
    void SetFlag(IDatabase& db, std::int64_t characterId, std::int64_t flagId);

    // Claims flagId only if it wasn't already set -- returns false if it
    // was. Used as the one-time-quest gate in handlers/Quest.cpp: two
    // pipelined CG_QUEST_RESULT turn-ins for the same character (see
    // GameSessionStore.h) both check has_flag against the same stale
    // pre-post snapshot, so without this the flag alone can't stop a
    // one-time quest's rewards from being granted twice. Claim this first,
    // before granting anything, and treat false as "already completed by
    // an earlier pipelined request" rather than a plain SetFlag no-op.
    bool TryClaimFlag(IDatabase& db, std::int64_t characterId, std::int64_t flagId);
} // namespace QuestFlagRepository
