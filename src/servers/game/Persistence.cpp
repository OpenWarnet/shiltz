#include "Persistence.h"

#include <exception>
#include <iostream>

Persistence::Persistence(IDatabase& db, ToWorld toWorld) : m_db(db), m_toWorld(std::move(toWorld))
{
}

Persistence::~Persistence()
{
    // Finish queued jobs instead of dropping them.
    m_thread.join();
}

void Persistence::Save(std::function<void(IDatabase&)> job, OnFailed onFailed)
{
    boost::asio::post(m_thread, [this, job = std::move(job), onFailed = std::move(onFailed)]() mutable
                      { Attempt([&] { job(m_db); }, std::move(onFailed)); });
}

bool Persistence::Attempt(const std::function<void()>& work, OnFailed onFailed)
{
    std::string error;
    try
    {
        work();
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
    catch (...)
    {
        error = "unknown exception";
    }

    std::cerr << "!! DB job failed: " << error << "\n";
    if (onFailed)
        m_toWorld([onFailed = std::move(onFailed), error = std::move(error)] { onFailed(error); });
    return false;
}
