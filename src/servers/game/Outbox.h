#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

struct ServerProtocol;

// Which connection a packet goes to. Currently the socket value.
using ConnectionId = std::uint64_t;

// The one way game code sends packets. Knows only connections.
// Safe from any thread.
class Outbox
{
public:
    using SendFn = std::function<void(ConnectionId, std::span<const std::uint8_t>)>;

    explicit Outbox(SendFn send);

    void Send(ConnectionId to, const ServerProtocol& message) const;

    // Serializes once, sends to each. `proj` maps an element to its
    // ConnectionId -- e.g. a pointer to the element's connection member.
    template <std::ranges::forward_range Targets, typename Proj = std::identity>
        requires std::convertible_to<
            std::invoke_result_t<Proj&, std::ranges::range_reference_t<const Targets>>,
            ConnectionId>
    void Send(const Targets& to, const ServerProtocol& message, Proj proj = {}) const
    {
        if (std::ranges::begin(to) == std::ranges::end(to))
            return;

        const std::vector<std::uint8_t> frame = Frame(message);
        for (const auto& target : to)
            m_send(std::invoke(proj, target), frame);
    }

private:
    static std::vector<std::uint8_t> Frame(const ServerProtocol& message);

    SendFn m_send;
};
