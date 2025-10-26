#include "concurrency/thread_pool.h"

#include <spdlog/spdlog.h>

namespace concurrency
{

thread_pool_t::thread_pool_t(std::size_t numThreads, std::string name)
    : m_name{std::move(name)}
{
    for (std::size_t threadIdx{0}; threadIdx < numThreads; ++threadIdx)
    {
        m_workers.emplace_back(
            [this, threadIdx]
            {
                std::string threadName{fmt::format("{}_{}", m_name, threadIdx)};
                pthread_setname_np(pthread_self(), threadName.c_str());

                this->workerLoop();
            }
        );
    }
}

thread_pool_t::~thread_pool_t()
{
    shutdown();
}

void thread_pool_t::shutdown()
{
    {
        std::unique_lock<std::mutex> lock{m_queueMutex};
        m_stop = true;
    }

    m_cv.notify_all();

    for (auto &worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

auto thread_pool_t::size() const -> std::size_t
{
    std::unique_lock<std::mutex> lock{m_queueMutex};
    return m_tasks.size();
}

void thread_pool_t::workerLoop()
{
    while (true)
    {
        std::move_only_function<void()> task;

        {
            std::unique_lock<std::mutex> lock{m_queueMutex};
            m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });

            if (m_stop && m_tasks.empty())
            {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        try
        {
            task();
        }
        catch (const std::exception &e)
        {
            spdlog::error(
                "thread_pool_t::workerLoop: ThreadPool {} task exception: {}", m_name, e.what()
            );
        }
    }
}

} // namespace concurrency
