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

#include <cstdio>

namespace world_v2::test
{

inline int g_failures = 0;

inline void Fail(const char* expression, const char* file, int line)
{
    std::printf("  FAIL  %s:%d\n        %s\n", file, line, expression);
    ++g_failures;
}

inline int Summary(const char* suite)
{
    if (g_failures == 0)
    {
        std::printf("PASS  %s\n", suite);
        return 0;
    }

    std::printf("FAIL  %s -- %d check(s) failed\n", suite, g_failures);
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
