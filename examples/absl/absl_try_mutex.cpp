#include <condition_variable>
#include <iostream>
#include <optional>
#include <queue>
#include <thread>
#include <vector>
#include <mutex>

#include <absl/synchronization/mutex.h>

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
        m_mutex.Await(
            absl::Condition(+[](std::vector<TItem> *q) { return !q->empty(); }, &m_queue));

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
    void print()
    {
        absl::MutexLock lock(&m_mutex);
        for (const auto &item : m_queue)
        {
            std::cout << item << std::endl;
        }
    }

  private:
    absl::Mutex        m_mutex;
    std::vector<TItem> m_queue;
};

class std_thq
{
  public:
    void push(int i)
    {
        std::lock_guard lock(m_mutex);
        m_queue.push(i);
        m_cv.notify_one();
    }

    std::optional<int> pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_cv.wait_for(lock, std::chrono::seconds(1), [this] { return !m_queue.empty(); }))
        {
            return std::nullopt;
        }
        int res = m_queue.front();
        m_queue.pop();
        return res;
    }

  private:
    std::mutex              m_mutex;
    std::condition_variable m_cv;
    std::queue<int>         m_queue;
};

void producer(std_thq &queue)
{
    int i{0};
    while (true)
    {
        queue.push(i++);

        if (i == 1'00)
        {
            break;
        }
    }
}

void consumer(std_thq &queue)
{
    for (;;)
    {
        auto item = queue.pop();
        if (item.has_value())
        {
            std::cout << "Consumed: " << item.value() << std::endl;
        }
        else
        {
            break;
        }
    }
}

auto main() -> int
{
    std_thq queue;

    auto producerThread = std::thread(producer, std::ref(queue));
    auto consumerThread = std::thread(consumer, std::ref(queue));

    producerThread.join();
    consumerThread.join();

    return 0;
}
