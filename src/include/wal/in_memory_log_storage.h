#pragma once

#include <vector>

#include <spdlog/spdlog.h>

#include "concepts.h"

namespace wal
{

template <typename TEntry> class in_memory_log_storage_impl_t
{
  public:
    using entry_type_t = TEntry;

    in_memory_log_storage_impl_t() = default;

    explicit in_memory_log_storage_impl_t(std::vector<std::string> m_log);

    in_memory_log_storage_impl_t(in_memory_log_storage_impl_t &&other) noexcept;

    auto operator=(in_memory_log_storage_impl_t &&other) noexcept -> in_memory_log_storage_impl_t &;

    in_memory_log_storage_impl_t(const in_memory_log_storage_impl_t &other) = delete;
    auto operator=(const in_memory_log_storage_impl_t &) -> in_memory_log_storage_impl_t & = delete;

    ~in_memory_log_storage_impl_t() noexcept = default;

    [[nodiscard]] auto append(TEntry entry) -> bool;
    [[nodiscard]] auto read(size_t index) const -> std::optional<entry_type_t>;
    [[nodiscard]] auto reset() -> bool;
    [[nodiscard]] auto size() const -> std::size_t;
    [[nodiscard]] auto reset_last_n(std::size_t n) -> bool;

  private:
    std::vector<TEntry> m_log;
};

template <typename TEntry>
inline auto in_memory_log_storage_impl_t<TEntry>::reset_last_n(std::size_t n) -> bool
{
    if (n == 0)
    {
        return false;
    }

    const auto logSize{m_log.size()};
    if (logSize < n)
    {
        spdlog::error(
            "in_memory_log_storage_impl_t::reset_last_n: Log size {} is smaller "
            "than provided {} removal size",
            logSize,
            n
        );
        return false;
    }

    m_log.resize(logSize - n);

    return true;
}

template <typename TEntry>
inline auto in_memory_log_storage_impl_t<TEntry>::size() const -> std::size_t
{
    return m_log.size();
}

template <typename TEntry> inline auto in_memory_log_storage_impl_t<TEntry>::reset() -> bool
{
    m_log.clear();
    return true;
}

template <typename TEntry>
inline auto in_memory_log_storage_impl_t<TEntry>::read(size_t index) const
    -> std::optional<entry_type_t>
{
    if (index < m_log.size())
    {
        return m_log[index];
    }
    return std::nullopt;
}

template <typename TEntry>
inline auto in_memory_log_storage_impl_t<TEntry>::append(TEntry entry) -> bool
{
    m_log.emplace_back(std::move(entry));
    return true;
}

template <typename TEntry>
inline auto
in_memory_log_storage_impl_t<TEntry>::operator=(in_memory_log_storage_impl_t &&other) noexcept
    -> in_memory_log_storage_impl_t &
{
    if (this == &other)
    {
        return *this;
    }
    m_log = std::move(other.m_log);
    return *this;
}

template <typename TEntry>
inline in_memory_log_storage_impl_t<TEntry>::in_memory_log_storage_impl_t(
    in_memory_log_storage_impl_t &&other
) noexcept
    : m_log{std::move(other.m_log)}
{
}

template <typename TEntry>
inline in_memory_log_storage_impl_t<TEntry>::in_memory_log_storage_impl_t(
    std::vector<std::string> m_log
)
    : m_log(std::move(m_log))
{
}

template <typename TEntry>
    requires TLogStorageConcept<in_memory_log_storage_impl_t, TEntry>
using in_memory_log_storage_t = in_memory_log_storage_impl_t<TEntry>;

class in_memory_storage_builder_t
{
  public:
    in_memory_storage_builder_t() = default;

    template <typename TEntry> [[nodiscard]] auto build()
    {
        return std::make_optional(in_memory_log_storage_t<TEntry>{});
    }
};

} // namespace wal
