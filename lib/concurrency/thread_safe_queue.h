#pragma once

#include <optional>
#include <vector>

#include <absl/synchronization/mutex.h>

namespace concurrency
{

template <typename TItem> class thread_safe_queue_t
{
  public:
    void push(TItem item)
    {
        absl::MutexLock lock(&m_mutex);
        m_queue.emplace_back(std::move(item));
    }

    auto pop() -> std::optional<TItem>
    {
        absl::MutexLock lock(&m_mutex);
        m_mutex.Await(absl::Condition(+[](std::vector<TItem> *q) { return !q->empty(); }, &m_queue));

        auto item = std::make_optional(m_queue.front());
        m_queue.erase(m_queue.begin());
        return item;
    }

    auto size() -> std::size_t
    {
        absl::MutexLock lock(&m_mutex);
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
    absl::Mutex        m_mutex;
    std::vector<TItem> m_queue;
};

} // namespace concurrency