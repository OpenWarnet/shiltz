#include "VisibilityIndex.h"

#include <algorithm>

void VisibilityIndex::Add(std::uint32_t instanceId, Zone::Coordinates playerZone)
{
    if (!Zone::IsInGrid(playerZone))
        return;

    for (const Zone::Coordinates visibleZone : Zone::Around(playerZone))
        Insert(m_viewers_by_zone[visibleZone.Index()], instanceId);
}

void VisibilityIndex::Move(std::uint32_t instanceId,
                           Zone::Coordinates from,
                           Zone::Coordinates to)
{
    if (from == to)
        return;

    Remove(instanceId, from);
    Add(instanceId, to);
}

void VisibilityIndex::Remove(std::uint32_t instanceId, Zone::Coordinates playerZone)
{
    if (!Zone::IsInGrid(playerZone))
        return;

    for (const Zone::Coordinates visibleZone : Zone::Around(playerZone))
        Erase(m_viewers_by_zone[visibleZone.Index()], instanceId);
}

std::span<const std::uint32_t>
VisibilityIndex::ViewersOf(Zone::Coordinates observedZone) const noexcept
{
    if (!Zone::IsInGrid(observedZone))
        return {};

    return m_viewers_by_zone[observedZone.Index()];
}

void VisibilityIndex::Insert(std::vector<std::uint32_t>& viewers, std::uint32_t instanceId)
{
    if (std::find(viewers.begin(), viewers.end(), instanceId) == viewers.end())
        viewers.push_back(instanceId);
}

void VisibilityIndex::Erase(std::vector<std::uint32_t>& viewers, std::uint32_t instanceId)
{
    std::erase(viewers, instanceId);
}
