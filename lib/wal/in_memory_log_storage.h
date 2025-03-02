#pragma once

#include <optional>
#include <vector>

#include "concepts.h"

// TODO(lnikon): Maybe resize() m_log in ctor to some default_log_size_mb?
class in_memory_log_storage_t
{
  public:
    in_memory_log_storage_t() = default;

    in_memory_log_storage_t(in_memory_log_storage_t &&) = default;
    in_memory_log_storage_t &operator=(in_memory_log_storage_t &&) = default;

    void append(std::string entry)
    {
        // std::lock_guard<std::mutex> lock(mutex_);
        m_log.emplace_back(std::move(entry));
    }

    // Declare read() as const. The mutex is mutable so that it can be locked in a const method.
    [[nodiscard]] auto read(size_t index) const -> std::optional<std::string>
    {
        // std::lock_guard<std::mutex> lock(mutex_);
        if (index < m_log.size())
        {
            return m_log[index];
        }
        return std::nullopt;
    }

    [[nodiscard]] auto reset() -> bool
    {
        m_log.clear();
        return false;
    }

  private:
    std::vector<std::string> m_log;
};

static_assert(TLogStorageConcept<in_memory_log_storage_t, std::string>, "in_memory_storage_t must satisfy TLogStorageConcept");

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
