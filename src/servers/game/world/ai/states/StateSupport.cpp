#include "StateSupport.h"

#include "common/Random.h"
#include "world/Grid.h"

#include <algorithm>

namespace creature_ai
{
bool AdvanceTimer(std::chrono::milliseconds& timer, std::chrono::milliseconds delta)
{
    if (timer > delta)
    {
        timer -= delta;
        return false;
    }

    timer = std::chrono::milliseconds::zero();
    return true;
}

std::chrono::milliseconds WithJitter(std::chrono::milliseconds base,
                                     std::chrono::milliseconds maximumJitter)
{
    return base + std::chrono::milliseconds(
                      static_cast<std::int64_t>(Random::Below(maximumJitter.count())));
}

std::uint32_t NormalizeMonsterValue(std::int64_t value)
{
    return static_cast<std::uint32_t>(std::clamp<std::int64_t>(value, 0, Grid::kMaxCoordinate));
}
} // namespace creature_ai

