#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

// Queued pub/sub.
//   bus.On<MoveEvent>().Register<&OnMove>();                    // OnMove(e)
//   bus.On<MoveEvent>().Register<&MovementSystem::OnMove>(sys); // sys.OnMove(e)
//   bus.Publish(MoveEvent{...});                                // any thread, only queues
//   bus.Dispatch();                                             // runs listeners in publish order
class EventBus
{
    template <typename Event> struct Listener
    {
        void (*call)(const void* bound, const Event& event);
        const void* bound;
    };

    // One per event type, created on first use. Events are kept by value in
    // their type's vector; m_log remembers which type each Publish was, so
    // Dispatch can replay them in publish order across types.
    struct HandlerBase
    {
        virtual ~HandlerBase() = default;
        virtual void TakePending() = 0;
        virtual void DispatchNext() = 0;
        virtual void ResetBatch() = 0;
    };

    template <typename Event> struct Handler final : HandlerBase
    {
        std::vector<Listener<Event>> listeners;
        std::vector<Event> pending; // guarded by m_mutex
        std::vector<Event> batch;   // the Dispatch in progress
        std::size_t next = 0;

        // Called once per log entry; only the first call per Dispatch swaps.
        void TakePending() override
        {
            if (batch.empty())
                batch.swap(pending);
        }

        void DispatchNext() override
        {
            const Event& event = batch[next++];
            for (const Listener<Event>& listener : listeners)
                listener.call(listener.bound, event);
        }

        void ResetBatch() override
        {
            batch.clear();
            next = 0;
        }
    };

public:
    template <typename Event> class Sink
    {
    public:
        // Candidate(event)
        template <auto Candidate> void Register()
        {
            static_assert(std::is_invocable_v<decltype(Candidate), const Event&>,
                          "listener must be callable as Candidate(const Event&)");
            m_listeners.push_back({
                [](const void*, const Event& event) { std::invoke(Candidate, event); },
                nullptr,
            });
        }

        // object.Candidate(event)
        template <auto Candidate, typename Object> void Register(Object& object)
        {
            static_assert(std::is_member_function_pointer_v<decltype(Candidate)>,
                          "Register(object) takes a member function; use Register() for "
                          "free/static functions");
            static_assert(std::is_invocable_v<decltype(Candidate), Object&, const Event&>,
                          "listener must be callable as object.Candidate(const Event&)");
            m_listeners.push_back({
                [](const void* bound, const Event& event)
                {
                    Object& target = *static_cast<Object*>(const_cast<void*>(bound));
                    std::invoke(Candidate, target, event);
                },
                std::addressof(object),
            });
        }

    private:
        friend EventBus;

        explicit Sink(std::vector<Listener<Event>>& listeners) : m_listeners(listeners) {}

        std::vector<Listener<Event>>& m_listeners;
    };

    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    template <typename Event> [[nodiscard]] Sink<Event> On()
    {
        static_assert(std::is_same_v<Event, std::remove_cvref_t<Event>>,
                      "use the plain event type -- On<MoveEvent>, not On<const MoveEvent&>");
        std::lock_guard lock(m_mutex);
        return Sink<Event>(Assure<Event>().listeners);
    }

    template <typename Event> void Publish(Event event)
    {
        std::lock_guard lock(m_mutex);
        Handler<Event>& handler = Assure<Event>();
        handler.pending.push_back(std::move(event));
        m_log.push_back(&handler);
    }

    void Dispatch()
    {
        {
            std::lock_guard lock(m_mutex);
            m_dispatching.swap(m_log);
            for (HandlerBase* handler : m_dispatching)
                handler->TakePending();
        }

        // Reset even if a listener throws, so the next Dispatch starts clean.
        struct Finish
        {
            std::vector<HandlerBase*>& dispatching;
            ~Finish()
            {
                for (HandlerBase* handler : dispatching)
                    handler->ResetBatch();
                dispatching.clear();
            }
        } finish{m_dispatching};

        for (HandlerBase* handler : m_dispatching)
            handler->DispatchNext();
    }

private:
    // Dense per-type index, shared by every bus: m_handlers[TypeIndex<E>()].
    static std::size_t NextTypeIndex()
    {
        static std::atomic<std::size_t> next{0};
        return next.fetch_add(1, std::memory_order_relaxed);
    }

    template <typename Event> static std::size_t TypeIndex()
    {
        static const std::size_t index = NextTypeIndex();
        return index;
    }

    // m_mutex must be held.
    template <typename Event> Handler<Event>& Assure()
    {
        const std::size_t index = TypeIndex<Event>();
        if (index >= m_handlers.size())
            m_handlers.resize(index + 1);

        std::unique_ptr<HandlerBase>& slot = m_handlers[index];
        if (!slot)
            slot = std::make_unique<Handler<Event>>();
        return static_cast<Handler<Event>&>(*slot);
    }

    std::mutex m_mutex;
    std::vector<std::unique_ptr<HandlerBase>> m_handlers; // guarded by m_mutex
    std::vector<HandlerBase*> m_log;                       // guarded by m_mutex
    std::vector<HandlerBase*> m_dispatching;
};
