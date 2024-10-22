#pragma once

#include <absl/time/time.h>
#include <algorithm>
#include <deque>
#include <optional>

#include <mutex>
#include <condition_variable>

#include <absl/synchronization/mutex.h>
#include <spdlog/spdlog.h>

namespace concurrency
{

template <typename TItem> class thread_safe_queue_t
{
  public:
    using queue_t = std::deque<TItem>;

    std::mutex              smut;
    std::condition_variable cv;

    void push(TItem item)
    {
        /*absl::WriterMutexLock lock(&m_mutex);*/
        /*spdlog::debug("Pushing item to the queue. size={}", m_queue.size());*/

        std::lock_guard<std::mutex> lk(smut);
        m_queue.emplace_back(std::move(item));
        cv.notify_one();
    }

    auto pop() -> std::optional<TItem>
    {
        /*absl::WriterMutexLock lock(&m_mutex);*/
        // if (!m_mutex.AwaitWithTimeout(
        //         absl::Condition(+[](queue_t *queue) { return !queue->empty(); }, &m_queue), absl::Seconds(1)))
        //{
        //     return std::nullopt;
        // }

        /*spdlog::debug("Popping item from the queue. size={}", m_queue.size());*/

        std::unique_lock<std::mutex> lk(smut);
        cv.wait_for(lk, std::chrono::seconds(1), [this] { return !m_queue.empty(); });
        auto item = std::make_optional(m_queue.front());
        m_queue.pop_front();
        return item;
    }

    auto pop_all() -> queue_t
    {
        absl::WriterMutexLock lock(&m_mutex);
        if (m_queue.empty())
        {
            return {};
        }

        return std::move(m_queue);
    }

    auto size() -> std::size_t
    {
        /*absl::ReaderMutexLock lock(&m_mutex);*/
        absl::MutexLock lock(&m_mutex);
        /*spdlog::debug("Getting queue size. size={}", m_queue.size());*/
        return m_queue.size();
    }

    template <typename TRecord, typename TKey = TRecord::Key>
    auto find(const TKey &recordKey) const noexcept -> std::optional<TRecord>
    {
        /*absl::ReaderMutexLock lock(&m_mutex);*/
        absl::MutexLock lock(&m_mutex);

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
    mutable absl::Mutex m_mutex;
    queue_t             m_queue;
};

} // namespace concurrency
