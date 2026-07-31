#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <utility>

// Opcode -> handler lookup, shared by the per-protocol dispatchers (see
// LoginDispatcher/GameDispatcher). Derive a protocol-specific dispatcher and
// populate the handler table via the base constructor, built with
// OpcodeBinder<Context, Packet>.
template <typename Context, typename Packet> class Dispatcher
{
public:
    using HandlerFn = std::function<void(const Context&, const Packet&)>;

    Dispatcher(std::initializer_list<std::pair<const uint32_t, HandlerFn>> handlers)
        : m_handlers(handlers)
    {
    }

    void Dispatch(const Context& ctx, const Packet& packet) const
    {
        auto it = m_handlers.find(packet.GetCode());
        if (it == m_handlers.end())
        {
            std::cout << "Received unknown packet code: " << std::hex << packet.GetCode() << std::dec
                      << "\n";
            return;
        }

        it->second(ctx, packet);
    }

private:
    std::unordered_map<uint32_t, HandlerFn> m_handlers;
};
