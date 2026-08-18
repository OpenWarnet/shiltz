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

template <typename Context, typename Packet, typename OpcodeT> class OpcodeBinder
{
public:
    using HandlerFn = std::function<void(const Context&, const Packet&)>;

    explicit OpcodeBinder(OpcodeT opcode) : m_opcode(opcode) {}

    template <typename TMessage> class Typed
    {
    public:
        explicit Typed(OpcodeT opcode) : m_opcode(opcode) {}

        std::pair<OpcodeT, HandlerFn>
        Then(std::function<void(const Context&, const TMessage&)> handler) const
        {
            return {m_opcode, [handler](const Context& ctx, const Packet& packet)
                    {
                        PayloadReader reader(packet.GetPayload());
                        TMessage message;
                        if (!message.Deserialize(reader))
                        {
                            std::cout << "Failed parsing request for opcode "
                                      << static_cast<uint32_t>(packet.GetCode()) << "\n";
                            return;
                        }
                        handler(ctx, message);
                    }};
        }

    private:
        OpcodeT m_opcode;
    };

    class Untyped
    {
    public:
        explicit Untyped(OpcodeT opcode) : m_opcode(opcode) {}

        std::pair<OpcodeT, HandlerFn> Then(std::function<void(const Context&)> handler) const
        {
            return {m_opcode, [handler](const Context& ctx, const Packet&) { handler(ctx); }};
        }

    private:
        OpcodeT m_opcode;
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
    OpcodeT m_opcode;
};
