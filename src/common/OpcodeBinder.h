#pragma once

#include "common/PayloadReader.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <utility>

template <typename Context, typename Packet> class OpcodeBinder
{
public:
    using HandlerFn = std::function<void(const Context&, const Packet&)>;

    explicit OpcodeBinder(uint32_t opcode) : m_opcode(opcode) {}

    template <typename TMessage> class Typed
    {
    public:
        explicit Typed(uint32_t opcode) : m_opcode(opcode) {}

        std::pair<uint32_t, HandlerFn>
        ThenHandle(std::function<void(const Context&, const TMessage&)> handler) const
        {
            return {m_opcode, [handler](const Context& ctx, const Packet& packet)
                    {
                        PayloadReader reader(packet.GetPayload());
                        TMessage message;
                        if (!message.Deserialize(reader))
                        {
                            std::cout << "Failed parsing request for opcode " << packet.GetCode()
                                      << "\n";
                            return;
                        }
                        handler(ctx, message);
                    }};
        }

    private:
        uint32_t m_opcode;
    };

    class Untyped
    {
    public:
        explicit Untyped(uint32_t opcode) : m_opcode(opcode) {}

        std::pair<uint32_t, HandlerFn> ThenHandle(std::function<void(const Context&)> handler) const
        {
            return {m_opcode, [handler](const Context& ctx, const Packet&) { handler(ctx); }};
        }

    private:
        uint32_t m_opcode;
    };

    template <typename TMessage> Typed<TMessage> ParseAs() const
    {
        return Typed<TMessage>(m_opcode);
    }

    // Payload exists but is intentionally not parsed yet.
    Untyped Ignore() const
    {
        return Untyped(m_opcode);
    }

    // Wire body is confirmed empty.
    Untyped Empty() const
    {
        return Untyped(m_opcode);
    }

private:
    uint32_t m_opcode;
};
