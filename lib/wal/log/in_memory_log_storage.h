#pragma once

#include "concepts.h"

#include <fmt/format.h>
#include <optional>
#include <vector>

namespace wal::log
{

// TODO(lnikon): Maybe resize() m_log in ctor to some default_log_size_mb?
class in_memory_log_storage_t
{
  public:
    // Used to construct an empty storage
    in_memory_log_storage_t() = default;

    explicit in_memory_log_storage_t(std::vector<std::string> m_log)
        : m_log(std::move(m_log))
    {
    }

    in_memory_log_storage_t(in_memory_log_storage_t &&other) noexcept
        : m_log{std::move(other.m_log)}
    {
    }

    auto operator=(in_memory_log_storage_t &&other) noexcept
        -> in_memory_log_storage_t &
    {
        if (this == &other)
        {
            return *this;
        }

        m_log = std::move(other.m_log);

        return *this;
    }

    in_memory_log_storage_t(const in_memory_log_storage_t &other) = delete;
    auto operator=(in_memory_log_storage_t &)
        -> in_memory_log_storage_t & = delete;

    ~in_memory_log_storage_t() noexcept = default;

    [[nodiscard]] auto append(std::string entry) -> bool
    {
        m_log.emplace_back(std::move(entry));
        return true;
    }

    [[nodiscard]] auto
    append(std::string command, std::string key, std::string value) -> bool
    {
        m_log.emplace_back(fmt::format("{} {} {}", command, key, value));
        return true;
    }

    [[nodiscard]] auto read(size_t index) const -> std::optional<std::string>
    {
        if (index < m_log.size())
        {
            return m_log[index];
        }
        return std::nullopt;
    }

    [[nodiscard]] auto reset() -> bool
    {
        m_log.clear();
        return true;
    }

    [[nodiscard]] auto size() const -> std::size_t
    {
        return m_log.size();
    }

  private:
    std::vector<std::string> m_log;
};

static_assert(TLogStorageConcept<in_memory_log_storage_t, std::string>,
              "in_memory_storage_t must satisfy TLogStorageConcept");

// --------------------------------------------------------
// Builder for in_memory_storage_builder_t (instance-based).
// --------------------------------------------------------
class in_memory_storage_builder_t
{
  public:
    in_memory_storage_builder_t() = default;

    [[nodiscard]] auto build() -> std::optional<in_memory_log_storage_t>
    {
        return in_memory_log_storage_t{};
    }
};

} // namespace wal::log
