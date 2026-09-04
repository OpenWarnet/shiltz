#pragma once

#include "core/TypeId.h"
#include "world/MapWorld.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace world_v2
{

// The one legal way for another thread to reach a MapWorld.
//
// Everything below this is single-threaded on purpose: Registry does no
// locking, and its pools reallocate as they grow, so a socket thread that
// assigned a component while the simulation was mid-sweep could invalidate
// a reference the sweep is standing on. There is no lock that fixes that
// cheaply, so the design does not try -- the simulation owns its state
// outright, and everyone else asks.
//
// A packet handler running on a connection thread turns the packet into a
// command value and Pushes it. Nothing happens yet. At the top of the next
// tick the simulation thread Drains the queue, and the registered handler
// for each command type runs there -- on the simulation thread, before any
// system has started iterating.
//
// Why commands are values, not callbacks
// --------------------------------------
// A queue of std::function<void(MapWorld&)> would be less code and is the
// obvious shortcut. It is also a lambda captured on a connection thread
// whose frame is gone by the time it runs, which makes a dangling capture
// an easy accident and an ugly crash. Command structs are copied into the
// queue, own everything they carry, and can be logged, replayed, and tested
// on their own.
//
// Entity handles in a command
// ---------------------------
// Fine to carry, but the handler must check Registry::Exists before acting.
// A player can disconnect, or a monster die, between the push and the
// drain; the generation in the handle is what turns that into a rejected
// command rather than a write to whoever inherited the slot.
//
// Threading contract
// ------------------
//   Push          any thread, any number of them
//   On            simulation thread, during setup, before ticking starts
//   Drain         simulation thread only, and never re-entered
//   PendingCount  any thread
class CommandQueue
{
public:
    CommandQueue() = default;

    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;

    // Registers what to do with a command of type T when it is drained.
    //
    // Not thread-safe and not meant to be: wire every handler up during
    // startup, before the first tick and before any connection thread
    // exists. One handler per type -- registering T twice replaces the
    // first, because two independent interpretations of the same inbound
    // command is a wiring mistake rather than a feature.
    template <typename T>
    void On(std::function<void(MapWorld&, const T&)> handler)
    {
        HandlerSlot<T>& slot = GetHandlerSlot<T>();
        slot.handler = std::move(handler);
    }

    // Queues `command` for the next Drain. Safe from any thread.
    //
    // Holds the lock only long enough to append -- the allocation happens
    // outside it, so connection threads are not serialized on each other's
    // heap work.
    template <typename T>
    void Push(T command)
    {
        std::unique_ptr<ICommand> wrapped = std::make_unique<Command<T>>(std::move(command));

        const std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.push_back(std::move(wrapped));
    }

    // Applies every queued command to `world` and returns how many ran.
    // Simulation thread only, at the top of the tick.
    //
    // The batch is swapped out under the lock and applied outside it, so a
    // handler doing real work never blocks a connection thread trying to
    // push. It also means anything pushed *while* this is running -- by
    // another thread, or by a handler itself -- belongs to the next tick,
    // not this one, which is what keeps a tick's inbound set a fixed thing
    // rather than a moving target.
    std::size_t Drain(MapWorld& world)
    {
        std::vector<std::unique_ptr<ICommand>> batch;
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            batch.swap(m_pending);
        }

        for (const std::unique_ptr<ICommand>& command : batch)
        {
            command->Apply(world, *this);
        }

        return batch.size();
    }

    std::size_t PendingCount() const
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_pending.size();
    }

    // Commands drained with no handler registered for their type. Unlike an
    // event nobody listens for, this is a wiring bug -- a packet made it
    // all the way here and then did nothing -- so it is counted rather than
    // silently dropped.
    std::size_t UnhandledCount() const
    {
        return m_unhandled;
    }

    void Clear()
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.clear();
    }

private:
    struct IHandlerSlot
    {
        virtual ~IHandlerSlot() = default;
    };

    template <typename T>
    struct HandlerSlot final : IHandlerSlot
    {
        std::function<void(MapWorld&, const T&)> handler;
    };

    struct ICommand
    {
        virtual ~ICommand() = default;
        virtual void Apply(MapWorld& world, CommandQueue& queue) const = 0;
    };

    template <typename T>
    struct Command final : ICommand
    {
        explicit Command(T value)
            : data(std::move(value))
        {
        }

        void Apply(MapWorld& world, CommandQueue& queue) const override
        {
            queue.Invoke<T>(world, data);
        }

        T data;
    };

    template <typename T>
    void Invoke(MapWorld& world, const T& command)
    {
        const TypeId id = TypeIdOf<CommandFamily>::Value<T>();
        if (id >= m_handlers.size() || !m_handlers[id])
        {
            ++m_unhandled;
            return;
        }

        HandlerSlot<T>& slot = *static_cast<HandlerSlot<T>*>(m_handlers[id].get());
        if (!slot.handler)
        {
            ++m_unhandled;
            return;
        }

        slot.handler(world, command);
    }

    template <typename T>
    HandlerSlot<T>& GetHandlerSlot()
    {
        const TypeId id = TypeIdOf<CommandFamily>::Value<T>();
        if (id >= m_handlers.size())
        {
            m_handlers.resize(id + 1);
        }

        if (!m_handlers[id])
        {
            m_handlers[id] = std::make_unique<HandlerSlot<T>>();
        }

        return *static_cast<HandlerSlot<T>*>(m_handlers[id].get());
    }

    // Written only during setup and read only on the simulation thread, so
    // it is deliberately outside the lock -- see the threading contract.
    std::vector<std::unique_ptr<IHandlerSlot>> m_handlers;

    mutable std::mutex m_mutex;
    std::vector<std::unique_ptr<ICommand>> m_pending;

    std::size_t m_unhandled = 0;
};

} // namespace world_v2
