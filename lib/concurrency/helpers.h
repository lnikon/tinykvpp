#pragma once

#include <absl/synchronization/mutex.h>

namespace concurrency
{

class absl_dual_mutex_lock_guard final
{
  public:
    absl_dual_mutex_lock_guard(absl::Mutex &mutex1, absl::Mutex &mutex2) noexcept;

    absl_dual_mutex_lock_guard() = delete;

    absl_dual_mutex_lock_guard(const absl_dual_mutex_lock_guard &) = delete;
    auto operator=(const absl_dual_mutex_lock_guard &) -> absl_dual_mutex_lock_guard & = delete;

    absl_dual_mutex_lock_guard(absl_dual_mutex_lock_guard &&) = delete;
    auto operator=(absl_dual_mutex_lock_guard &&) -> absl_dual_mutex_lock_guard & = delete;

    ~absl_dual_mutex_lock_guard() noexcept;

  private:
    absl::Mutex *m_first_mutex{nullptr};
    absl::Mutex *m_second_mutex{nullptr};
};

} // namespace concurrency
