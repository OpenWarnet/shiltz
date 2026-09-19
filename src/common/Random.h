#pragma once

#include <atomic>
#include <cstdint>

// Fast, non-cryptographic randomness for game logic -- spawn scatter, AI
// rolls, jitter. Safe to call from any thread.
//
// Next() mixes a process-wide counter through SplitMix64. That mix is a
// bijection on 64 bits, so distinct counter values give distinct results:
// no two Next() calls anywhere in the process -- same tick, same thread or
// not -- ever return the same number. Values derived from it with Below()
// repeat freely, as a bounded roll must.
namespace Random
{
// SplitMix64's finalizer. Exposed on its own for callers that need a
// reproducible number from data they already hold rather than a fresh one.
[[nodiscard]] inline constexpr std::uint64_t Mix(std::uint64_t seed) noexcept
{
    seed += 0x9E3779B97F4A7C15ULL;
    seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ULL;
    seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBULL;
    return seed ^ (seed >> 31);
}

[[nodiscard]] inline std::uint64_t Next() noexcept
{
    static std::atomic<std::uint64_t> counter{0};
    return Mix(counter.fetch_add(1, std::memory_order_relaxed));
}

// A roll in [0, bound). Returns 0 for an empty range.
[[nodiscard]] inline std::uint64_t Below(std::uint64_t bound) noexcept
{
    return bound == 0 ? 0 : Next() % bound;
}
} // namespace Random
