#pragma once

#include <absl/time/time.h>
#include <optional>
#include <queue>

#include <absl/synchronization/mutex.h>
#include <spdlog/spdlog.h>

namespace concurrency
{

template <typename TItem> class thread_safe_queue_t
{
  public:
    void push(TItem item)
    {
        absl::MutexLock lock(&m_mutex);
        spdlog::debug("Pushing item to the queue. size={}", m_queue.size());
        m_queue.push(std::move(item));
    }

    auto pop() -> std::optional<TItem>
    {
        absl::MutexLock lock(&m_mutex);
        if (!m_mutex.AwaitWithTimeout(
                absl::Condition(+[](std::queue<TItem> *queue) { return !queue->empty(); }, &m_queue), absl::Seconds(1)))
        {
            return std::nullopt;
        }

        spdlog::debug("Popping item from the queue. size={}", m_queue.size());
        auto item = std::make_optional(m_queue.front());
        m_queue.pop();
        return item;
    }

    auto pop_all() -> std::queue<TItem>
    {
        absl::MutexLock lock(&m_mutex);
        if (m_queue.empty())
        {
            return {};
        }

        return std::move(m_queue);
    }

    auto size() -> std::size_t
    {
        absl::MutexLock lock(&m_mutex);
        spdlog::debug("Getting queue size. size={}", m_queue.size());
        return m_queue.size();
    }

    // For debugging purposes
    // void print()
    // {
    //     absl::MutexLock lock(&m_mutex);
    //     for (const auto &item : m_queue)
    //     {
    //         std::cout << item << std::endl;
    //     }
    // }

  private:
    absl::Mutex       m_mutex;
    std::queue<TItem> m_queue;
};

} // namespace concurrency