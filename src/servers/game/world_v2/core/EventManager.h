#pragma once

#include "TypeId.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace world_v2
{

// Type-safe, deferred event broker.
//
// Emit() records an event and returns immediately; nothing runs until
// Flush(). That gap is the whole point. Systems sweep packed component
// arrays and must not restructure them mid-sweep -- so a system that
// decides "this monster died" emits a note instead of destroying anything,
// and Flush(), called at the tick's synchronization barrier, is where the
// structural work actually happens.
//
// Ordering
// --------
// One queue holds every pending event in emit order regardless of type, so
// a DeathEvent emitted before a LevelUpEvent dispatches before it, run
// after run. (Bucketing by type in a hash map -- the obvious alternative --
// would make the order across types depend on hash iteration order, which
// is not something a server whose behavior has to be reproducible can
// afford.) Listeners for the same event type fire in registration order.
//
// Cascades
// --------
// Handlers may emit. Flush() keeps going in rounds until the queue drains,
// so death -> loot drop -> level-up all settles inside a single barrier
// rather than trickling out one link per tick.
//
// kMaxFlushRounds bounds that in case two handlers feed each other forever.
// On hitting the cap, Flush() returns with the still-pending events left in
// the queue -- they go out on the next tick's Flush rather than being
// dropped, so a deep-but-terminating cascade degrades into a delay instead
// of losing events.
//
// Cost
// ----
// Each Emit heap-allocates a small wrapper to erase the event's type. Fine
// at the volumes a barrier actually sees; if a profile ever says otherwise,
// the fix is per-type value vectors plus a separate order log, which is why
// callers only ever touch Listen/Emit/Flush.
//
// Single-threaded, like Registry -- same thread, same tick.
class EventManager
{
public:
    static constexpr int kMaxFlushRounds = 8;

    EventManager() = default;

    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    // Registers a callback for every T emitted from here on. Subscriptions
    // last for the life of the EventManager; there is no unsubscribe,
    // because listeners are wired up once at startup by the systems
    // themselves and never torn down independently.
    template <typename T>
    void Listen(std::function<void(const T&)> callback)
    {
        ListenerList<T>& list = GetListeners<T>();
        list.callbacks.push_back(std::move(callback));
    }

    // Records `event` for dispatch at the next Flush. The event is copied
    // (or moved) into the queue, so the caller's copy can go out of scope
    // immediately -- events are values, never references into the registry.
    template <typename T>
    void Emit(T event)
    {
        m_pending.push_back(std::make_unique<PendingEvent<T>>(std::move(event)));
    }

    // Dispatches everything queued, then everything those handlers queued,
    // and so on -- see the class comment on rounds and the cap.
    //
    // This is the one place in a tick where structural registry changes are
    // safe. Call it at the barrier, never from inside a system sweep.
    void Flush()
    {
        for (int round = 0; round < kMaxFlushRounds; ++round)
        {
            if (m_pending.empty())
            {
                return;
            }

            // Move the batch out first: handlers running below will push
            // into m_pending, and those belong to the *next* round, not
            // this one. Iterating m_pending directly would mean iterating a
            // vector that the handlers are reallocating.
            std::vector<std::unique_ptr<IPendingEvent>> batch = std::move(m_pending);
            m_pending.clear();

            for (const std::unique_ptr<IPendingEvent>& event : batch)
            {
                event->Dispatch(*this);
            }
        }
    }

    std::size_t PendingCount() const
    {
        return m_pending.size();
    }

    // Drops queued events without dispatching them. For teardown and for
    // tests; a normal tick drains through Flush.
    void Clear()
    {
        m_pending.clear();
    }

private:
    struct IListenerList
    {
        virtual ~IListenerList() = default;
    };

    template <typename T>
    struct ListenerList final : IListenerList
    {
        std::vector<std::function<void(const T&)>> callbacks;
    };

    struct IPendingEvent
    {
        virtual ~IPendingEvent() = default;
        virtual void Dispatch(EventManager& manager) const = 0;
    };

    template <typename T>
    struct PendingEvent final : IPendingEvent
    {
        explicit PendingEvent(T value)
            : data(std::move(value))
        {
        }

        // Nested classes may reach the enclosing class's private members,
        // which is what lets this call back into Invoke.
        void Dispatch(EventManager& manager) const override
        {
            manager.Invoke<T>(data);
        }

        T data;
    };

    template <typename T>
    void Invoke(const T& event)
    {
        const TypeId id = TypeIdOf<EventFamily>::Value<T>();
        if (id >= m_listeners.size() || !m_listeners[id])
        {
            // Emitting something nobody listens for is normal, not an
            // error: a system announces what happened; whether anything
            // cares is somebody else's wiring.
            return;
        }

        ListenerList<T>& list = *static_cast<ListenerList<T>*>(m_listeners[id].get());

        // Index rather than range-for: a handler is allowed to Listen for
        // this same event type, which would reallocate the vector under an
        // iterator. Newly added callbacks deliberately do not run for the
        // event already in flight.
        const std::size_t count = list.callbacks.size();
        for (std::size_t i = 0; i < count; ++i)
        {
            list.callbacks[i](event);
        }
    }

    template <typename T>
    ListenerList<T>& GetListeners()
    {
        const TypeId id = TypeIdOf<EventFamily>::Value<T>();
        if (id >= m_listeners.size())
        {
            m_listeners.resize(id + 1);
        }

        if (!m_listeners[id])
        {
            m_listeners[id] = std::make_unique<ListenerList<T>>();
        }

        return *static_cast<ListenerList<T>*>(m_listeners[id].get());
    }

    // Indexed by TypeIdOf<EventFamily>::Value<T>() -- a vector index, not a
    // hash lookup, for the same reason Registry's pools are. See TypeId.h.
    std::vector<std::unique_ptr<IListenerList>> m_listeners;

    // One queue for every type, preserving global emit order.
    std::vector<std::unique_ptr<IPendingEvent>> m_pending;
};

} // namespace world_v2
