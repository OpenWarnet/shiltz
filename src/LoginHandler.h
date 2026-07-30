#pragma once
#include <cstdint>
#include <vector>
#include <winsock2.h>
#include <span>

class LoginPacket;
class DESCipher;

class LoginHandler {
private:
	SOCKET m_clientSocket;
    std::span<const uint8_t> m_key;

public:
    LoginHandler(SOCKET clientSocket, std::span<const uint8_t> key);

    bool Handle(LoginPacket packet);
};