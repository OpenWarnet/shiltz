#include "World.h"

#include <iostream>

void World::Start()
{
    std::cout << "World started\n";
}

void World::Shutdown()
{
    std::cout << "World shut down\n";
}

Map& World::GetMap()
{
    return m_map;
}

const Map& World::GetMap() const
{
    return m_map;
}
