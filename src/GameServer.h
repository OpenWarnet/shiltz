#pragma once

#include "GameDispatcher.h"
#include "common/TCPServer.h"

#include <cstdint>
#include <span>

class GameServer : public TCPServer
{
public:
    GameServer(uint16_t port, std::span<const uint8_t> key);

protected:
    void OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame) override;

private:
    GameDispatcher m_dispatcher;
    std::span<const uint8_t> m_key;
};
