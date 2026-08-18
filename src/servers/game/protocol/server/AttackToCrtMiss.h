#pragma once

#include <cstdint>

class PayloadWriter;

// GC_ATTACK_TO_CRT_MISS (wire code 531018, s2c) -- rejects CG_ATTACK_TO_CRT
// with a miss. 8 bytes, u32 + i32. No live capture exists for this one --
// field roles are traced only from the reference binary's own
// scratch-buffer writes right before it queues the packet. First slot is
// the target creature's instance_id (the client already knows who's
// attacking -- it's the client's own request -- so it only needs to be
// told which target the miss applies to). Second slot's role is still
// unresolved; signed since the source binary's scratch global isn't known
// to be non-negative-only.
struct AttackToCrtMiss
{
    std::uint32_t target_instance_id = 0;
    std::int32_t unknown = 0;

    void Serialize(PayloadWriter& writer) const;
};
