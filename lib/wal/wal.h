#pragma once

#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>
#include <libassert/assert.hpp>
#include <absl/strings/ascii.h>

#include "wal/common.h"
#include "log/log.h"

namespace wal
{

using namespace log;

// TODO(lnikon): Because the wal_t is generic, this alias should be moved into db_t. Entry type used
// by WAL log
using wal_entry_t = record_t;

template <typename TEntry> class wal_t
{
  public:
    using wal_entry_t = TEntry;

    wal_t() = delete;
    explicit wal_t(log_t<TEntry> log) noexcept;

    wal_t(wal_t &&other) noexcept;
    auto operator=(wal_t &&other) noexcept -> wal_t &;

    wal_t(const wal_t &other) = delete;
    auto operator=(const wal_t &other) -> wal_t & = delete;

    ~wal_t() = default;

    [[nodiscard]] auto add(const wal_entry_t &rec) noexcept -> bool;
    [[nodiscard]] auto records() const noexcept -> std::vector<wal_entry_t>;
    [[nodiscard]] auto read(std::size_t index) const noexcept -> std::optional<wal_entry_t>;

    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;

    auto reset() noexcept -> bool;

  private:
    log_t<TEntry> m_log;
};

template <typename TEntry> using shared_ptr_t = std::shared_ptr<wal_t<TEntry>>;

template <typename TEntry, typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<wal_t<TEntry>>(std::forward<Args>(args)...);
}

enum class wal_builder_error_t : std::uint8_t
{
    kUndefined,
    kLogBuildFailed,
    kInvalidConfiguration,
};

template <typename TEntry>
wal_t<TEntry>::wal_t(log_t<TEntry> log) noexcept
    : m_log{std::move(log)}
{
}

template <typename TEntry>
wal_t<TEntry>::wal_t(wal_t &&other) noexcept
    : m_log(std::move(other.m_log))
{
}

template <typename TEntry> auto wal_t<TEntry>::operator=(wal_t &&other) noexcept -> wal_t &
{
    if (this == &other)
    {
        return *this;
    }
    m_log = std::move(other.m_log);
    return *this;
}

template <typename TEntry>
auto wal_t<TEntry>::add(const wal_t<TEntry>::wal_entry_t &rec) noexcept -> bool
{
    return m_log.append(rec);
}

template <typename TEntry> auto wal_t<TEntry>::reset() noexcept -> bool
{
    return m_log.reset();
}

template <typename TEntry>
[[nodiscard]] auto wal_t<TEntry>::records() const noexcept
    -> std::vector<wal_t<TEntry>::wal_entry_t>
{
    if (m_log.size() == 0)
    {
        spdlog::info("WAL: Log is empty. Nothing to recover.");
        return {};
    }

    std::vector<wal_entry_t> result;
    result.reserve(m_log.size());
    for (std::size_t idx{0}; idx < m_log.size(); ++idx)
    {
        auto entry{m_log.read(idx)};
        if (!entry)
        {
            spdlog::error("wal_t::records: Got nullopt as an entry");
            continue;
        }
        result.emplace_back(std::move(*entry));
    }
    return result;
}

template <typename TEntry>
[[nodiscard]] auto wal_t<TEntry>::read(std::size_t index) const noexcept
    -> std::optional<wal_t<TEntry>::wal_entry_t>
{
    return m_log.read(index);
}

template <typename TEntry> [[nodiscard]] auto wal_t<TEntry>::size() const noexcept -> std::size_t
{
    return m_log.size();
}

template <typename TEntry> [[nodiscard]] auto wal_t<TEntry>::empty() const noexcept -> bool
{
    return size() == 0;
}

template <typename TEntry> class wal_builder_t
{
  public:
    using return_type_t = std::expected<wal::wal_t<TEntry>, wal::wal_builder_error_t>;

    wal_builder_t() = default;

    // Build method that creates the appropriate wal_t instance
    auto build(log_t<TEntry> log) -> std::expected<wal_t<TEntry>, wal_builder_error_t>
    {
        return wal_t(std::move(log));
    }
};

} // namespace wal
