#pragma once

#include "common/PacketCapture.h"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string_view>
#include <unordered_map>
#include <utility>

// Opcode -> handler lookup, shared by the per-protocol dispatchers (see
// LoginDispatcher/GameDispatcher). Derive a protocol-specific dispatcher and
// populate the handler table via the base constructor, built with
// OpcodeBinder<Context, Packet>. Every dispatched frame is also routed
// through PacketCapture so devs have a decrypted, opcode-named record of
// traffic -- including opcodes with no handler yet.
template <typename Context, typename Packet, typename OpcodeT> class Dispatcher
{
public:
    using HandlerFn = std::function<void(const Context&, const Packet&)>;
    using NameResolverFn = std::function<std::string_view(OpcodeT)>;

    Dispatcher(NameResolverFn opcodeNameResolver,
               std::initializer_list<std::pair<const OpcodeT, HandlerFn>> handlers)
        : m_opcodeNameResolver(std::move(opcodeNameResolver)), m_handlers(handlers)
    {
    }

    void Dispatch(const Context& ctx, const Packet& packet) const
    {
        auto it = m_handlers.find(packet.GetCode());
        auto name = m_opcodeNameResolver(packet.GetCode());

        if (it == m_handlers.end())
        {
            PacketCapture::LogUnhandled(ctx.clientSocket, static_cast<uint32_t>(packet.GetCode()), name,
                                         packet.GetPayload());
            return;
        }

        PacketCapture::LogHandled(PacketCapture::Direction::Inbound, ctx.clientSocket,
                                   static_cast<uint32_t>(packet.GetCode()), name, packet.GetPayload());
        it->second(ctx, packet);
    }

private:
    NameResolverFn m_opcodeNameResolver;
    std::unordered_map<OpcodeT, HandlerFn> m_handlers;
};
