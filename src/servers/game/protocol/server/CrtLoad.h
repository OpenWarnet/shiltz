#pragma once

#include <array>
#include <cstdint>
#include <vector>

class PayloadWriter;

// A single GC_CRT_LOAD entity/creature record (one "slot") -- 112 bytes on
// the wire: id, x, y, monster_id, direction, hp, followed by 21 reserved
// uint32 fields that are always zero.
struct CrtLoadRecord
{
    std::uint32_t id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t monster_id = 0;
    std::uint32_t direction = 0;
    std::uint64_t hp = 0;
    std::array<std::uint32_t, 21> reserved{};

    void Serialize(PayloadWriter& writer) const;
};

// GC_CRT_LOAD (wire code 0x0007cc35 / 511029, s2c) -- nearby entity/creature
// spawn list, sent as part of the post-CG_ENTER load burst. Wire shape is
// `count` (u32) followed by `count` 112-byte CrtLoadRecord entries -- see
// OpenShiltz's game/handlers/gc_crt_load.py.
struct CrtLoad
{
    std::vector<CrtLoadRecord> records;

    void Serialize(PayloadWriter& writer) const;
};
