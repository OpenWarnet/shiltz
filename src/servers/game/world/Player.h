#pragma once

#include <cstdint>
#include <utility>
#include <vector>

// A connected client's live position/facing in the simulation -- kept as
// part of its GameSession (see GameSessionStore.h) and updated in
// real-time from CG_MOVE, unlike Creature (world/Creature.h), which is
// placed once at load and never moves. `instance_id` is the same value as
// CharacterDataLoad::self_entity_id (the character's own id, reused as its
// entity id on the wire).
struct Player
{
    std::uint32_t instance_id = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t direction = 0;

    // The zones (see Map::ZonesAround) this player was last sent
    // GC_CRT_LOAD/GC_VIEW_REMOVE_ALL for -- diffed against the new set on
    // each move to know which zones' creatures to load/unload.
    std::vector<std::pair<std::int32_t, std::int32_t>> known_zones;

    // Live simulation stats -- same fields CharacterDataLoad/QuestSucc
    // currently send as hardcoded placeholders (see handlers/Session.cpp,
    // handlers/Quest.cpp). Not yet persisted to or loaded from the DB, and
    // not yet the source of truth for those responses; this is just the
    // in-memory home for them until handlers are wired to read/write here.
    std::int64_t money = 0;
    std::uint32_t hp = 1;
    std::uint32_t ap = 1;
    std::uint32_t fame = 0;
    std::int64_t exp = 1;
};
