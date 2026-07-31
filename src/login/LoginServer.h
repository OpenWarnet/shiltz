#pragma once

#include "LoginDispatcher.h"
#include "common/TCPServer.h"

#include <cstdint>
#include <span>

class LoginServer : public TCPServer
{
public:
    LoginServer(uint16_t port, std::span<const uint8_t> key, std::span<const uint8_t> noncePayload);

protected:
    void OnClientConnected(SOCKET clientSocket) override;
    void OnFrame(SOCKET clientSocket, std::span<const uint8_t> frame) override;

private:
    LoginDispatcher m_dispatcher;
    std::span<const uint8_t> m_key;
    std::span<const uint8_t> m_noncePayload;
};
