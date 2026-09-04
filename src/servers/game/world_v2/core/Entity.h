#pragma once

#include <cstdint>

namespace world_v2
{

// A handle to one game object. Deliberately an opaque 32-bit value rather
// than a plain index: the low bits are a slot index into the Registry's
// arrays, the high bits a generation counter that Registry bumps every time
// that slot is freed.
//
//     [ 31 .. 20 : generation (12 bits) ][ 19 .. 0 : index (20 bits) ]
//
// The generation is what makes a *stale* handle detectable. Without it,
// destroying monster A and spawning monster B would hand B the same integer
// A had, and any handle to A still sitting in an AIComponent::currentTarget
// from last tick would silently start pointing at B. With it, the freed
// slot's generation moves on, the old handle no longer matches, and
// Registry::Exists rejects it -- see Registry::Destroy.
//
// The split caps the world at 1,048,575 simultaneously-live entities
// (index 0xFFFFF is reserved so that an all-ones handle can mean "none"),
// and lets a slot be recycled 4,096 times before its generation wraps and a
// truly ancient handle could alias again. Both are far past anything a
// single map server needs; if either ever binds, move the bit boundary or
// widen Entity to 64 bits -- every accessor below goes through these
// helpers, so nothing else has to change.
using Entity = std::uint32_t;

inline constexpr std::uint32_t kEntityIndexBits = 20;
inline constexpr std::uint32_t kEntityGenerationBits = 32 - kEntityIndexBits;

inline constexpr std::uint32_t kEntityIndexMask = (1u << kEntityIndexBits) - 1;
inline constexpr std::uint32_t kEntityGenerationMask = (1u << kEntityGenerationBits) - 1;

// The largest index Registry may hand out. One below the mask, because the
// all-ones index belongs to kNullEntity.
inline constexpr std::uint32_t kMaxEntityIndex = kEntityIndexMask - 1;

// The "no entity" handle -- what an unset AIComponent::currentTarget or a
// failed lookup holds. Never returned by Registry::Create, and always false
// from Registry::Exists.
inline constexpr Entity kNullEntity = ~0u;

constexpr std::uint32_t EntityIndex(Entity entity)
{
    return entity & kEntityIndexMask;
}

constexpr std::uint32_t EntityGeneration(Entity entity)
{
    return (entity >> kEntityIndexBits) & kEntityGenerationMask;
}

constexpr Entity MakeEntity(std::uint32_t index, std::uint32_t generation)
{
    return ((generation & kEntityGenerationMask) << kEntityIndexBits) | (index & kEntityIndexMask);
}

} // namespace world_v2
