#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <winsock2.h>

class TCPServer;
class GamePacket;

struct GameContext
{
    TCPServer& server;
    SOCKET clientSocket;
    std::span<const uint8_t> key;
};

class GameDispatcher
{
public:
    using HandlerFn = std::function<void(const GameContext& ctx, const GamePacket& packet)>;

    GameDispatcher();

    void Dispatch(const GameContext& ctx, const GamePacket& packet) const;

private:
    std::unordered_map<uint32_t, HandlerFn> m_handlers;
};
