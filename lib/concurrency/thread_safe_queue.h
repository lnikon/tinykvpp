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

    thread_safe_queue_t(thread_safe_queue_t &&other) noexcept
        : m_queue{move_under_optional_lock(other.m_queue, other.m_mutex)}
    {
    }

    auto operator=(thread_safe_queue_t &&other) noexcept -> thread_safe_queue_t &
    {
        if (this != &other)
        {
            absl_dual_mutex_lock_guard guard{m_mutex, other.m_mutex};
            thread_safe_queue_t        temp{std::move(other)};
            swap(other);
        }
        return *this;
    }

    ~thread_safe_queue_t() noexcept = default;

    void push(TItem item)
    {
        absl::WriterMutexLock lock(&m_mutex);
        spdlog::debug("Pushing item to the queue. size={}", m_queue.size());
        m_queue.emplace_back(std::move(item));
    }

    auto pop() -> std::optional<TItem>
    {
        absl::WriterMutexLock lock(&m_mutex);
        if (!m_mutex.AwaitWithTimeout(
                absl::Condition(
                    +[](queue_t *queue) { return !queue->empty(); }, &m_queue),
                absl::Seconds(1)))
        {
            return std::nullopt;
        }

        spdlog::debug("Popping item from the queue. size={}", m_queue.size());
        auto item = std::make_optional(m_queue.front());
        m_queue.pop_front();
        return item;
    }

    auto pop_all() -> queue_t
    {
        absl::WriterMutexLock lock(&m_mutex);
        return m_queue.empty() ? queue_t{} : std::move(m_queue);
    }

    auto size() -> std::size_t
    {
        absl::ReaderMutexLock lock(&m_mutex);
        spdlog::debug("Getting queue size. size={}", m_queue.size());
        return m_queue.size();
    }

    template <typename TRecord, typename TKey = TRecord::Key>
    auto find(const TKey &recordKey) const noexcept -> std::optional<TRecord>
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

  private:
    void swap(thread_safe_queue_t &other) noexcept
    {
        using std::swap;

        // Mutex is not movable.
        swap(m_queue, other.m_queue);
    }

    mutable absl::Mutex m_mutex;
    queue_t             m_queue;
};

} // namespace concurrency
