#pragma once

#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <string>
#include <vector>
#include <pthread.h>

#include <fmt/format.h>

namespace concurrency
{

class thread_pool_t final
{
  public:
    explicit thread_pool_t(std::size_t numThreads, std::string name);

    thread_pool_t(const thread_pool_t &&) = delete;
    auto operator==(const thread_pool_t &&) -> thread_pool_t & = delete;

    thread_pool_t(thread_pool_t &) = delete;
    auto operator==(thread_pool_t &) -> thread_pool_t & = delete;

    ~thread_pool_t();

    template <typename F, typename... Args>
    auto enqueue(F &&f, Args &&...args) -> std::future<typename std::invoke_result_t<F, Args...>>
    {
        using return_type = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock{m_queueMutex};
            m_cv.wait(lock, [this] { return !m_stop || !m_tasks.empty(); });

            if (m_stop)
            {
                throw std::runtime_error("enqueue on stopped thread pool");
            }

            m_tasks.emplace([task] { (*task)(); });
        }

        m_cv.notify_one();
        return res;
    }

    void shutdown();

    [[nodiscard]] auto size() const -> std::size_t;

  private:
    void workerLoop();

    std::string m_name;

    std::vector<std::thread>                    m_workers;
    std::queue<std::move_only_function<void()>> m_tasks;

    mutable std::mutex      m_queueMutex;
    std::condition_variable m_cv;
    bool                    m_stop{false};
};

} // namespace concurrency
