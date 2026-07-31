#pragma once

#include <array>
#include <cstdint>
#include <vector>

class PayloadWriter;

// A single GC_CRT_LOAD entity/creature record -- 112 bytes on the wire.
// Field layout confirmed against a real capture in OpenShiltz's
// game/handlers/gc_crt_load.py (16 real frames, `4 + count*112 ==
// body length` held exactly for every one). `id`/`type` are directly
// debug-string-confirmed there ("<GC_CRT_LOAD> Invalid Monster type=%ld,
// id=%ld"); `hp`/`f4`/`pos1..3` and `appearance_raw`'s internal layout carry
// lower confidence -- see that file's docstring for exactly what's
// confirmed vs. inferred.
struct CrtLoadRecord
{
    std::uint32_t id = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t type = 0;
    std::uint32_t spawn_count = 0;
    std::uint64_t hp = 0;
    std::uint32_t f4 = 0;
    std::uint32_t pos1 = 0;
    std::uint32_t pos2 = 0;
    std::uint32_t pos3 = 0;
    std::array<std::uint8_t, 68> appearance_raw{};

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
