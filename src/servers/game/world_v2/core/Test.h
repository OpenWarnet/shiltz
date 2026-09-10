#pragma once

// Minimal check-and-tally harness for the world_v2 core tests.
//
// Deliberately dependency-free: vcpkg.json pins only what the servers
// themselves need, and none of these tests are worth a test-framework
// dependency for. Each *.test.cpp is its own executable with a main() that
// ends in `return world_v2::test::Summary("name");`, registered with
// add_test so `ctest` runs the lot.
//
// Checks don't abort -- a failing CHECK records the file/line and keeps
// going, so one run reports every broken case instead of only the first.

#include <atomic>
#include <cstdio>

namespace world_v2::test
{

// Atomic because some tests drive several simulations on separate threads
// -- one per map, which is the arrangement Map is designed for -- and
// record failures from all of them. The printf itself is stream-locked, so
// two threads failing at once interleave lines rather than corrupting the
// tally.
inline std::atomic<int> g_failures{0};

inline void Fail(const char* expression, const char* file, int line)
{
    std::printf("  FAIL  %s:%d\n        %s\n", file, line, expression);
    g_failures.fetch_add(1, std::memory_order_relaxed);
}

inline int Summary(const char* suite)
{
    const int failures = g_failures.load(std::memory_order_relaxed);

    if (failures == 0)
    {
        std::printf("PASS  %s\n", suite);
        return 0;
    }

    std::printf("FAIL  %s -- %d check(s) failed\n", suite, failures);
    return 1;
}

} // namespace world_v2::test

#define CHECK(expression)                                                       \
    do                                                                          \
    {                                                                           \
        if (!(expression))                                                      \
        {                                                                       \
            ::world_v2::test::Fail(#expression, __FILE__, __LINE__);            \
        }                                                                       \
    } while (false)

#define CHECK_EQ(actual, expected) CHECK((actual) == (expected))
