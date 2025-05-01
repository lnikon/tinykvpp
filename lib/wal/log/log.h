#pragma once

#include "concepts.h"
#include "storage/builder.h"

namespace wal::log
{

template <typename TEntry> class log_impl_t
{
  public:
    using entry_type = TEntry;

    log_impl_t() = delete;

    explicit log_impl_t(storage::log_storage_wrapper_t<TEntry> storage) noexcept
        : m_storage(std::move(storage))
    {
    }

    log_impl_t(log_impl_t &&other) noexcept
        : m_storage{std::move(other.m_storage)}
    {
    }

    auto operator=(log_impl_t &&other) noexcept -> log_impl_t &
    {
        if (this != &other)
        {
            using std::swap;
            swap(*this, other);
        }
        return *this;
    }

    log_impl_t(const log_impl_t &) noexcept = delete;
    auto operator=(const log_impl_t &) noexcept -> log_impl_t & = delete;

    ~log_impl_t() noexcept = default;

    [[nodiscard]] auto append(entry_type entry) noexcept -> bool
    {
        return m_storage.append(std::move(entry));
    }

    [[nodiscard]] auto read(std::size_t index) const noexcept -> std::optional<entry_type>
    {
        return m_storage.read(index);
    }

    [[nodiscard]] auto reset() noexcept -> bool
    {
        return m_storage.reset();
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t
    {
        return m_storage.size();
    }

    [[nodiscard]] auto reset_last_n(std::size_t n) -> bool
    {
        return m_storage.reset_last_n(n);
    }

  private:
    storage::log_storage_wrapper_t<entry_type> m_storage;
};

template <typename TEntry>
    requires TLogStorageConcept<log_impl_t, TEntry>
using log_t = log_impl_t<TEntry>;

class log_builder_t
{
  public:
    log_builder_t() = default;

    template <typename TEntry>
    [[nodiscard]] auto build(storage::log_storage_wrapper_t<TEntry> storage) const
        -> std::optional<log_t<TEntry>>
    {
        return log_t{std::move(storage)};
    }
};

} // namespace wal::log
