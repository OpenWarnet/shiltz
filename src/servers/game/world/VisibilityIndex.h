#pragma once

#include "Zone.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

// Spatial subscription index for players. Each zone stores the instance ids of
// players whose 3x3 view includes it; Map resolves them to Players.
class VisibilityIndex
{
public:
    void Add(std::uint32_t instanceId, Zone::Coordinates playerZone);
    void Move(std::uint32_t instanceId, Zone::Coordinates from, Zone::Coordinates to);
    void Remove(std::uint32_t instanceId, Zone::Coordinates playerZone);

    [[nodiscard]] std::span<const std::uint32_t>
    ViewersOf(Zone::Coordinates observedZone) const noexcept;

private:
    static void Insert(std::vector<std::uint32_t>& viewers, std::uint32_t instanceId);
    static void Erase(std::vector<std::uint32_t>& viewers, std::uint32_t instanceId);

    std::array<std::vector<std::uint32_t>, Zone::kCount> m_viewers_by_zone;
};
