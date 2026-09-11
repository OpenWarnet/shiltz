#pragma once

#include "GameContext.h"
#include "protocol/ClientProtocol.h"

#include <memory>

struct Request
{
    GameContext context;
    std::unique_ptr<ClientProtocol> message;
};
