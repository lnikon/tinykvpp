#include <libassert/assert.hpp>

#include "concurrency/helpers.h"

namespace concurrency
{

absl_dual_mutex_lock_guard::absl_dual_mutex_lock_guard(
    absl::Mutex &mutex1, absl::Mutex &mutex2
) noexcept
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

absl_dual_mutex_lock_guard::~absl_dual_mutex_lock_guard() noexcept
{
    m_second_mutex->Unlock();
    m_first_mutex->Unlock();
}

} // namespace concurrency
