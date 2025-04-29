#pragma once

#include <libassert/assert.hpp>
#include <absl/synchronization/mutex.h>

namespace concurrency
{

// Useful utility to avoid deadlocks
class absl_dual_mutex_lock_guard final
{
  public:
    absl_dual_mutex_lock_guard() = delete;

    absl_dual_mutex_lock_guard(absl::Mutex &mutex1, absl::Mutex &mutex2) noexcept
    {
        ASSERT(&mutex1 != &mutex2, "same mutex passed into absl_dual_mutex_lock_guard");
        if (&mutex1 < &mutex2)
        {
            m_first_mutex = &mutex1;
            m_second_mutex = &mutex2;
        }
        else
        {
            m_first_mutex = &mutex2;
            m_second_mutex = &mutex1;
        }

        m_first_mutex->Lock();
        m_second_mutex->Lock();
    }

    absl_dual_mutex_lock_guard(const absl_dual_mutex_lock_guard &) = delete;
    auto operator=(const absl_dual_mutex_lock_guard &) -> absl_dual_mutex_lock_guard & = delete;

    absl_dual_mutex_lock_guard(absl_dual_mutex_lock_guard &&) = delete;
    auto operator=(absl_dual_mutex_lock_guard &&) -> absl_dual_mutex_lock_guard & = delete;

    ~absl_dual_mutex_lock_guard() noexcept
    {
        m_second_mutex->Unlock();
        m_first_mutex->Unlock();
    }

  private:
    absl::Mutex *m_first_mutex{nullptr};
    absl::Mutex *m_second_mutex{nullptr};
};

// Use inside initializer list of a move-constructor to perform 'defensive' locking in debug builds
template <typename T> auto move_under_optional_lock(T &obj, absl::Mutex &mutex)
{
    (void)mutex;
#ifdef THREAD_SAFE_QUEUE_DEFENSIVE_MOVE
    absl::WriterMutexLock lock(&mutex);
#endif
    return std::move(obj);
}

} // namespace concurrency
