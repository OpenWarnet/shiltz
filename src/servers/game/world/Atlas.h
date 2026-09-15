#pragma once

#include "Map.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class GameData;

// One Map's contribution to an Atlas::Tick() call -- which map, and every
// creature that moved on it this tick.
struct MapTickResult
{
    std::int64_t server_map_id = 0;
    std::vector<CreatureMove> creature_moves;
};

// Fixed-capacity collection of every live Map in one World. server_map_id
// is used as a direct index, while empty IDs occupy only a null pointer.
class Atlas
{
public:
    explicit Atlas(std::size_t maxMapId = 600);

    // Non-positive IDs are unused map.scr slots and are ignored. Throws if
    // the ID is out of range, duplicated, or the Map cannot load its data.
    void Add(MapRecord record, const GameData& data);

    Map* Get(std::int64_t mapId);
    const Map* Get(std::int64_t mapId) const;

    void Tick(std::chrono::milliseconds delta);

    // Calls fn(Map&) for every loaded map.
    template <typename Fn> void ForEach(Fn&& fn)
    {
        for (auto& map : m_maps)
        {
            if (map)
                fn(*map);
        }
    }

    std::size_t Size() const noexcept;
    bool Empty() const noexcept;

private:
    std::vector<std::unique_ptr<Map>> m_maps;
    std::size_t m_mapCount = 0;
};
