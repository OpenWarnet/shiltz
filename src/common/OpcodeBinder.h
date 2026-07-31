#pragma once

#include "common/PayloadReader.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <utility>

// Why an opcode's payload isn't parsed into a message type.
enum class SkipReason
{
    Empty,   // Wire body is confirmed empty.
    Ignored, // Payload exists but is intentionally not parsed yet.
};

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
        Then(std::function<void(const Context&, const TMessage&)> handler) const
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

        std::pair<uint32_t, HandlerFn> Then(std::function<void(const Context&)> handler) const
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

    Untyped SkipParse(SkipReason) const
    {
        return Untyped(m_opcode);
    }

private:
    uint32_t m_opcode;
};
