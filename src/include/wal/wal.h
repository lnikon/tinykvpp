#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "wal/common.h"
#include "wal/in_memory_log_storage.h"
#include "wal/persistent_log_storage.h"

namespace wal
{

template <typename TEntry>
using variant_t = std::variant<
    // In-memory log storage
    in_memory_log_storage_t<TEntry>,
    // Append-only file-based log storage
    persistent_log_storage_t<backend::append_only_file_storage_backend_t, TEntry>>;

template <typename TEntry> class wal_t
{
  public:
    using entry_type_t = TEntry;

    explicit wal_t(variant_t<TEntry> log);

    wal_t() = delete;

    wal_t(wal_t &&other) noexcept;
    auto operator=(wal_t &&other) noexcept -> wal_t &;

    wal_t(const wal_t &other) = delete;
    auto operator=(const wal_t &other) -> wal_t & = delete;

    ~wal_t() = default;

    [[nodiscard]] auto add(entry_type_t entry) -> bool;
    [[nodiscard]] auto read(size_t index) const -> std::optional<entry_type_t>;
    [[nodiscard]] auto reset() -> bool;
    [[nodiscard]] auto size() const -> std::size_t;
    [[nodiscard]] auto reset_last_n(std::size_t n) -> bool;

    [[nodiscard]] auto records() const noexcept -> std::vector<entry_type_t>;
    [[nodiscard]] auto empty() const noexcept -> bool;

  private:
    variant_t<TEntry> m_log;
};

template <typename TEntry> inline auto wal_t<TEntry>::reset_last_n(std::size_t n) -> bool
{
    return std::visit([&](auto &storage) { return storage.reset_last_n(n); }, m_log);
}

template <typename TEntry> inline auto wal_t<TEntry>::size() const -> std::size_t
{
    return std::visit([&](auto &storage) { return storage.size(); }, m_log);
}

template <typename TEntry> inline auto wal_t<TEntry>::reset() -> bool
{
    return std::visit([&](auto &storage) { return storage.reset(); }, m_log);
}

template <typename TEntry>
inline auto wal_t<TEntry>::read(size_t index) const -> std::optional<entry_type_t>
{
    return std::visit([&](auto &storage) { return storage.read(index); }, m_log);
}

template <typename TEntry> inline auto wal_t<TEntry>::add(entry_type_t entry) -> bool
{
    return std::visit([&](auto &storage) -> auto { return storage.append(entry); }, m_log);
}

template <typename TEntry>
inline wal_t<TEntry>::wal_t(variant_t<TEntry> log)
    : m_log{std::move(log)}
{
}

template <typename TEntry> using shared_ptr_t = std::shared_ptr<wal_t<TEntry>>;

template <typename TEntry, typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<wal_t<TEntry>>(std::forward<Args>(args)...);
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
[[nodiscard]] auto wal_t<TEntry>::records() const noexcept
    -> std::vector<wal_t<TEntry>::entry_type_t>
{
    if (empty())
    {
        spdlog::info("WAL: Log is empty. Nothing to recover.");
        return {};
    }

    std::vector<entry_type_t> result;
    result.reserve(size());

    for (std::size_t idx{0}; idx < size(); ++idx)
    {
        auto entry{read(idx)};
        if (!entry.has_value())
        {
            spdlog::error("wal_t::records: Got nullopt as an entry");
            continue;
        }

        result.emplace_back(std::move(*entry));
    }

    return result;
}

template <typename TEntry> [[nodiscard]] auto wal_t<TEntry>::empty() const noexcept -> bool
{
    return size() == 0;
}

class wal_builder_t final
{
  public:
    auto set_file_path(fs::path_t path) -> wal_builder_t &;

    template <typename TEntry>
    [[nodiscard]] auto build(log_storage_type_k type) noexcept
        -> std::optional<wal::shared_ptr_t<TEntry>>
    {
        if (type == log_storage_type_k::in_memory_k)
        {
            return std::make_shared<wal_t<TEntry>>(
                variant_t<TEntry>{in_memory_log_storage_t<TEntry>{}}
            );
        }

        if (type == log_storage_type_k::file_based_persistent_k)
        {
            if (m_path.empty())
            {
                spdlog::error("wal_builder_t::build: Empty path specified");
                return std::nullopt;
            }

            if (auto storage = persistent_log_storage_builder_t<
                                   backend::append_only_file_storage_backend_t,
                                   TEntry>{{.file_path = m_path}}
                                   .build();
                storage.has_value())
            {
                return std::make_shared<wal_t<TEntry>>(
                    variant_t<TEntry>{std::move(storage.value())}
                );
            }

            return std::nullopt;
        }

        return std::nullopt;
    }

  private:
    fs::path_t m_path;
};

} // namespace wal
