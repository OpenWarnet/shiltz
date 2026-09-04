#pragma once

// Minimal timing harness for the world_v2 benchmarks.
//
// Dependency-free, like Test.h, and deliberately small: it times a callable
// repeatedly and reports the best and median run. Best is the least
// noise-contaminated estimate of what the code costs; median says whether
// the machine was quiet enough for that to mean anything. When the two are
// far apart, distrust the number rather than the code.
//
// Only meaningful in a release build. A debug MSVC build spends most of its
// time in iterator checking and container instrumentation, which swamps
// every difference these benchmarks exist to show.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace world_v2::bench
{

using Clock = std::chrono::steady_clock;

// Keeps a computed value from being optimized away. Every benchmark body
// returns a checksum that ends up here, so the compiler cannot delete the
// loop it came from.
inline void Sink(std::uint64_t value)
{
    volatile std::uint64_t sink = value;
    (void)sink;
}

struct Timing
{
    double bestNs = 0.0;
    double medianNs = 0.0;
};

// Runs `fn` `repeats` times and returns the best and median wall time in
// nanoseconds. `fn` must return a checksum derived from the work it did.
template <typename Fn>
Timing Measure(int repeats, Fn&& fn)
{
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(repeats));

    for (int i = 0; i < repeats; ++i)
    {
        const Clock::time_point start = Clock::now();
        Sink(fn());
        const Clock::time_point end = Clock::now();

        samples.push_back(std::chrono::duration<double, std::nano>(end - start).count());
    }

    std::sort(samples.begin(), samples.end());

    Timing timing;
    timing.bestNs = samples.front();
    timing.medianNs = samples[samples.size() / 2];
    return timing;
}

// Same, but with per-run setup that is not counted.
//
// Needed for anything that consumes the state it runs on -- a tick where a
// thousand entities die leaves nothing to kill on the second repeat, so the
// world has to be rebuilt between runs without the rebuild landing in the
// measurement.
template <typename Setup, typename Fn>
Timing MeasureWithSetup(int repeats, Setup&& setup, Fn&& fn)
{
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(repeats));

    for (int i = 0; i < repeats; ++i)
    {
        setup();

        const Clock::time_point start = Clock::now();
        Sink(fn());
        const Clock::time_point end = Clock::now();

        samples.push_back(std::chrono::duration<double, std::nano>(end - start).count());
    }

    std::sort(samples.begin(), samples.end());

    Timing timing;
    timing.bestNs = samples.front();
    timing.medianNs = samples[samples.size() / 2];
    return timing;
}

inline void Header(const char* section)
{
    std::printf("\n%s\n", section);
    std::printf("  %-46s %12s %12s %12s\n", "case", "best (us)", "median (us)", "ns/op");
}

// `ops` is what one run processed -- entities visited, lookups done -- so
// the ns/op column compares like with like across cases of different size.
inline void Row(const char* label, std::size_t ops, Timing timing)
{
    const double perOp = ops == 0 ? 0.0 : timing.bestNs / static_cast<double>(ops);
    std::printf("  %-46s %12.1f %12.1f %12.2f\n", label, timing.bestNs / 1000.0, timing.medianNs / 1000.0, perOp);
}

inline void Note(const char* text)
{
    std::printf("  -- %s\n", text);
}

} // namespace world_v2::bench
