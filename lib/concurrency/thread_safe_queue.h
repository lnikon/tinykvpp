#pragma once

#include <algorithm>
#include <deque>
#include <optional>

#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>
#include <spdlog/spdlog.h>

#include "helpers.h"

namespace concurrency
{

template <typename TItem> class thread_safe_queue_t
{
  public:
    using queue_t = std::deque<TItem>;

    thread_safe_queue_t() = default;

    thread_safe_queue_t(const thread_safe_queue_t &) = delete;
    auto operator=(const thread_safe_queue_t &) -> thread_safe_queue_t & = delete;

    thread_safe_queue_t(thread_safe_queue_t &&other) noexcept;
    auto operator=(thread_safe_queue_t &&other) noexcept -> thread_safe_queue_t &;

    ~thread_safe_queue_t() noexcept = default;

    void push(TItem item);

    auto pop() -> std::optional<TItem>;
    auto pop_all() -> queue_t;

    auto size() -> std::size_t;

    template <typename TRecord, typename TKey = TRecord::Key>
    auto find(const TKey &recordKey) const noexcept -> std::optional<TRecord>;

  private:
    mutable absl::Mutex m_mutex;
    queue_t             m_queue;
};

template <typename TItem>
inline thread_safe_queue_t<TItem>::thread_safe_queue_t(thread_safe_queue_t &&other) noexcept
    : m_queue{
          [&other]() -> queue_t
          {
              absl::WriterMutexLock lock{&other.m_mutex};
              return std::move(other.m_queue);
          }
      }
{
}

template <typename TItem>
inline auto thread_safe_queue_t<TItem>::operator=(thread_safe_queue_t &&other) noexcept
    -> thread_safe_queue_t &
{
    if (this != &other)
    {
        absl_dual_mutex_lock_guard lock{m_mutex, other.m_mutex};
        m_queue = std::move(other.m_queue);
    }
    return *this;
}

template <typename TItem>
template <typename TRecord, typename TKey>
inline auto thread_safe_queue_t<TItem>::find(const TKey &recordKey) const noexcept
    -> std::optional<TRecord>
{
    absl::ReaderMutexLock lock(&m_mutex);

    for (const auto &memtable : m_queue)
    {
        if (auto record = memtable.find(recordKey); record.has_value())
        {
            return record;
        }
    }

    return std::nullopt;
}

template <typename TItem> inline auto thread_safe_queue_t<TItem>::size() -> std::size_t
{
    absl::ReaderMutexLock lock(&m_mutex);
    spdlog::debug("Getting queue size. size={}", m_queue.size());
    return m_queue.size();
}

template <typename TItem> inline auto thread_safe_queue_t<TItem>::pop_all() -> queue_t
{
    absl::WriterMutexLock lock(&m_mutex);
    return m_queue.empty() ? queue_t{} : std::move(m_queue);
}

template <typename TItem> inline auto thread_safe_queue_t<TItem>::pop() -> std::optional<TItem>
{
    absl::WriterMutexLock lock(&m_mutex);
    if (!m_mutex.AwaitWithTimeout(
            absl::Condition(
                +[](queue_t *queue) { return !queue->empty(); }, &m_queue
            ),
            absl::Seconds(1)
        ))
    {
        return std::nullopt;
    }

    spdlog::debug("Popping item from the queue. size={}", m_queue.size());
    auto item = std::make_optional(m_queue.front());
    m_queue.pop_front();
    return item;
}

template <typename TItem> inline void thread_safe_queue_t<TItem>::push(TItem item)
{
    absl::WriterMutexLock lock(&m_mutex);
    spdlog::debug("Pushing item to the queue. size={}", m_queue.size());
    m_queue.emplace_back(std::move(item));
}

} // namespace concurrency
