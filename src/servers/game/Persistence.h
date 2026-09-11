#pragma once

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

class IDatabase;

// Runs DB jobs in order on one thread; results and failures come back on the world thread.
class Persistence
{
public:
    using ToWorld = std::function<void(std::function<void()>)>;
    using OnFailed = std::function<void(const std::string& error)>;

    Persistence(IDatabase& db, ToWorld toWorld);
    ~Persistence();

    Persistence(const Persistence&) = delete;
    Persistence& operator=(const Persistence&) = delete;

    // Fire-and-forget; onFailed (world thread) says the write didn't land.
    void Save(std::function<void(IDatabase&)> job, OnFailed onFailed = {});

    // Runs job, then onDone(result) or onFailed(error) on the world thread.
    template <typename Job, typename OnDone> void Run(Job job, OnDone onDone, OnFailed onFailed)
    {
        boost::asio::post(m_thread, [this, job = std::move(job), onDone = std::move(onDone),
                                     onFailed = std::move(onFailed)]() mutable {
            using Result = std::invoke_result_t<Job&, IDatabase&>;
            if constexpr (std::is_void_v<Result>)
            {
                if (Attempt([&] { job(m_db); }, std::move(onFailed)))
                    m_toWorld(std::move(onDone));
            }
            else
            {
                std::optional<Result> result;
                if (Attempt([&] { result.emplace(job(m_db)); }, std::move(onFailed)))
                {
                    m_toWorld([onDone = std::move(onDone), value = std::move(*result)]() mutable
                              { onDone(std::move(value)); });
                }
            }
        });
    }

    // Legacy handlers still post raw work here, so every DB job shares one ordered thread.
    boost::asio::thread_pool& Thread() { return m_thread; }

private:
    // Runs work, reporting any exception to onFailed; true if it succeeded.
    bool Attempt(const std::function<void()>& work, OnFailed onFailed);

    IDatabase& m_db;
    ToWorld m_toWorld;
    boost::asio::thread_pool m_thread{1};
};
