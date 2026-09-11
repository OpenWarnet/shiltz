#pragma once

#include <mutex>
#include <utility>
#include <vector>

// Thread-safe producer queue with a single bulk-consumer operation.
// DrainTo swaps the pending items into consumer-owned storage so the
// mutex is not held while the batch is processed.
template <typename T> class BatchQueue
{
public:
    BatchQueue() = default;

    BatchQueue(const BatchQueue&) = delete;
    BatchQueue& operator=(const BatchQueue&) = delete;
    BatchQueue(BatchQueue&&) = delete;
    BatchQueue& operator=(BatchQueue&&) = delete;

    void Push(T value)
    {
        std::lock_guard lock(m_mutex);
        m_pending.push_back(std::move(value));
    }

    void DrainTo(std::vector<T>& batch)
    {
        // Destroy the previously processed items outside the producer lock.
        // Its retained capacity is then handed back to m_pending by swap.
        batch.clear();

        std::lock_guard lock(m_mutex);
        batch.swap(m_pending);
    }

private:
    std::mutex m_mutex;
    std::vector<T> m_pending;
};
