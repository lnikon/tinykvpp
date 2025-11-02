#pragma once

#include <absl/base/thread_annotations.h>
#include <algorithm>
#include <deque>
#include <iterator>
#include <optional>

#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>
#include <spdlog/spdlog.h>

#include "db/db_config.h"
#include "helpers.h"

namespace concurrency
{

template <typename TItem> class thread_safe_queue_t
{
  public:
    using queue_t = std::deque<TItem>;

    thread_safe_queue_t() noexcept;

    thread_safe_queue_t(const thread_safe_queue_t &) = delete;
    auto operator=(const thread_safe_queue_t &) -> thread_safe_queue_t & = delete;

    thread_safe_queue_t(thread_safe_queue_t &&other) noexcept;
    auto operator=(thread_safe_queue_t &&other) noexcept -> thread_safe_queue_t &;

    ~thread_safe_queue_t() noexcept = default;

    auto push(TItem item) -> bool;

    auto pop() -> std::optional<TItem>;
    auto pop_all() -> queue_t;

    // Transactional interface.
    // NOLINTBEGIN(modernize-use-trailing-return-type)
    /**
     * Returns stored items in oldest-to-newest order (queue in a reversed order).
     */
    std::optional<std::vector<TItem>> reserve() noexcept ABSL_LOCKS_EXCLUDED(m_mutex);
    bool                              consume() noexcept ABSL_LOCKS_EXCLUDED(m_mutex);

    // NOLINTEND(modernize-use-trailing-return-type)

    auto size() -> std::size_t;

    template <typename TRecord, typename TKey = TRecord::Key>
    auto find(const TKey &recordKey) const noexcept -> std::optional<TRecord>;

    void shutdown();

  private:
    std::atomic<bool> m_shutdown{false};

    mutable absl::Mutex m_mutex;

    queue_t m_queue                              ABSL_GUARDED_BY(m_mutex);
    std::optional<std::uint64_t> m_reservedIndex ABSL_GUARDED_BY(m_mutex);
};

template <typename TItem>
inline thread_safe_queue_t<TItem>::thread_safe_queue_t() noexcept
    : m_reservedIndex{std::nullopt}
{
}

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

template <typename TItem> inline auto thread_safe_queue_t<TItem>::push(TItem item) -> bool
{
    [[likely]] if (!m_shutdown.load())
    {
        absl::WriterMutexLock lock(&m_mutex);
        m_queue.emplace_back(std::move(item));
        return true;
    }
    return false;
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

    auto item = std::make_optional(std::move(m_queue.front()));
    m_queue.pop_front();
    return item;
}

template <typename TItem> inline auto thread_safe_queue_t<TItem>::pop_all() -> queue_t
{
    absl::WriterMutexLock lock(&m_mutex);
    spdlog::info("thread_safe_queue_t<TItem>::pop_all()");
    return m_queue.empty() ? queue_t{} : std::move(m_queue);
}

template <typename TItem>
inline auto thread_safe_queue_t<TItem>::reserve() noexcept -> std::optional<std::vector<TItem>>
{
    absl::WriterMutexLock lock{&m_mutex};

    if (m_queue.empty())
    {
        spdlog::debug("thread_safe_queue: Queue is empty. Nothing to reserve");
        return std::nullopt;
    }

    if (m_reservedIndex.has_value())
    {
        spdlog::error("thread_safe_queue: Another reservation is in progress");
        return std::nullopt;
    }
    m_reservedIndex = m_queue.size();

    return std::make_optional<std::vector<TItem>>(std::rbegin(m_queue), std::rend(m_queue));
}

template <typename TItem> inline auto thread_safe_queue_t<TItem>::consume() noexcept -> bool
{
    absl::WriterMutexLock lock{&m_mutex};

    if (!m_reservedIndex.has_value())
    {
        spdlog::error("thread_safe_queue: No reservation to consume");
        return false;
    }

    const auto consumeUpTo{m_reservedIndex.value()};
    ASSERT(consumeUpTo != 0);
    for (std::uint64_t idx{0}; idx < consumeUpTo; idx++)
    {
        m_queue.pop_front();
    }
    m_reservedIndex = std::nullopt;

    return true;
}

template <typename TItem> inline auto thread_safe_queue_t<TItem>::size() -> std::size_t
{
    absl::ReaderMutexLock lock(&m_mutex);
    return m_queue.size();
}

template <typename TItem>
template <typename TRecord, typename TKey>
inline auto thread_safe_queue_t<TItem>::find(const TKey &recordKey) const noexcept
    -> std::optional<TRecord>
{
    absl::ReaderMutexLock lock(&m_mutex);

    if (m_queue.empty())
    {
        spdlog::info("MEMTABLES ARE EMPTY");
    }

    for (const auto &memtable : m_queue)
    {
        if (auto record = memtable.find(recordKey); record.has_value())
        {
            return record;
        }
    }

    return std::nullopt;
}

template <typename TItem> void thread_safe_queue_t<TItem>::shutdown()
{
    m_shutdown.store(true);
}

} // namespace concurrency
