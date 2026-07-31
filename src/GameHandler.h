#pragma once
#include <cstdint>
#include <vector>
#include <winsock2.h>
#include <span>

class GamePacket;

class GameHandler {
private:
	SOCKET m_clientSocket;
    std::span<const uint8_t> m_key;

public:
    GameHandler(SOCKET clientSocket, std::span<const uint8_t> key);

    bool Handle(GamePacket packet);
};