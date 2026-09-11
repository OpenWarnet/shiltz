#pragma once

#include <atomic>
#include <cstdint>

// Runtime id for anything that exists in the world and is addressed by id on
// the wire -- creatures, characters, ground items, ... One counter shared by
// every entity kind and every map, so an id is unique across the whole game
// server for the life of the process. Starts at 1; 0 means "not assigned".
// Safe to call from any thread.
namespace EntityIdGenerator
{
inline std::uint32_t Next()
{
    static std::atomic<std::uint32_t> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}
} // namespace EntityIdGenerator
