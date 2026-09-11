#pragma once

#include "Atlas.h"
#include "world/common/BatchQueue.h"
#include "world/common/Request.h"

#include <chrono>
#include <cstdint>
#include <vector>

class World
{
public:
    void Start();
    void Shutdown();

    Map* GetMap(std::int64_t id);
    const Map* GetMap(std::int64_t id) const;

    void Receive(Request request);
    void Tick(std::chrono::milliseconds delta);

private:
    BatchQueue<Request> ingress;

    std::vector<Request> m_requests;
    Atlas m_atlas;
};
