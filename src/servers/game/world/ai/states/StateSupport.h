#pragma once

#include <chrono>
#include <cstdint>

namespace creature_ai
{
bool AdvanceTimer(std::chrono::milliseconds& timer, std::chrono::milliseconds delta);

std::chrono::milliseconds WithJitter(std::chrono::milliseconds base,
                                     std::chrono::milliseconds maximumJitter);

std::uint32_t NormalizeMonsterValue(std::int64_t value);
} // namespace creature_ai

