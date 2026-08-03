#pragma once

#include "common/PacketCapture.h"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <utility>

// Opcode -> handler lookup, shared by the per-protocol dispatchers (see
// LoginDispatcher/GameDispatcher). Derive a protocol-specific dispatcher and
// populate the handler table via the base constructor, built with
// OpcodeBinder<Context, Packet>. Every dispatched frame is also routed
// through PacketCapture so devs have a decrypted, opcode-named record of
// traffic -- including opcodes with no handler yet.
template <typename Context, typename Packet> class Dispatcher
{
public:
    using HandlerFn = std::function<void(const Context&, const Packet&)>;
    using NameResolverFn = std::function<std::string_view(uint32_t)>;

    Dispatcher(NameResolverFn opcodeNameResolver,
               std::initializer_list<std::pair<const uint32_t, HandlerFn>> handlers)
        : m_opcodeNameResolver(std::move(opcodeNameResolver)), m_handlers(handlers)
    {
    }

    void Dispatch(const Context& ctx, const Packet& packet) const
    {
        auto it = m_handlers.find(packet.GetCode());
        auto name = m_opcodeNameResolver(packet.GetCode());

        if (it == m_handlers.end())
        {
            PacketCapture::LogUnhandled(ctx.clientSocket, packet.GetCode(), name, packet.GetPayload());
            std::cout << "Received unknown packet code: " << std::hex << packet.GetCode() << std::dec
                      << "\n";
            return;
        }

        PacketCapture::LogHandled(PacketCapture::Direction::Inbound, ctx.clientSocket,
                                   packet.GetCode(), name, packet.GetPayload());
        it->second(ctx, packet);
    }

private:
    NameResolverFn m_opcodeNameResolver;
    std::unordered_map<uint32_t, HandlerFn> m_handlers;
};
