#include "World.h"

#include "world/common/Paths.h"
#include "parser/MapScr.h"

#include <exception>
#include <iostream>
#include <utility>

void World::Start()
{
    std::cout << "World started\n";

    for (auto& record : MapScr::Load(Paths::Data.root / "map.scr"))
    {
        try
        {
            m_atlas.Add(std::move(record));
        }
        catch (const std::exception& e)
        {
            std::cout << "Skipping map: " << e.what() << "\n";
        }
    }
}

void World::Shutdown()
{
    std::cout << "World shut down\n";
}

Map* World::GetMap(std::int64_t id)
{
    return m_atlas.Get(id);
}

const Map* World::GetMap(std::int64_t id) const
{
    return m_atlas.Get(id);
}

void World::Receive(Request request)
{
    ingress.Push(std::move(request));
}

void World::Tick(std::chrono::milliseconds delta)
{
    ingress.DrainTo(m_requests);
    for (const auto& request : m_requests)
        request.message->Handle(request.context);

    m_atlas.Tick(delta);
}
