#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <winsock2.h>

class TCPServer;
class LoginPacket;

struct LoginContext
{
    TCPServer& server;
    SOCKET clientSocket;
    std::span<const uint8_t> key;
};

class LoginDispatcher
{
public:
    using HandlerFn = std::function<void(const LoginContext& ctx, const LoginPacket& packet)>;

    LoginDispatcher();

    void Dispatch(const LoginContext& ctx, const LoginPacket& packet) const;

private:
    std::unordered_map<uint32_t, HandlerFn> m_handlers;
};
