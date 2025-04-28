#pragma once

#include "concepts.h"
#include "wal/log/storage/builder.h"

namespace wal::log
{

class log_t
{
  public:
    log_t() = delete;

    explicit log_t(storage::log_storage_wrapper_t storage) noexcept
        : m_storage(std::move(storage))
    {
    }

    log_t(log_t &&other) noexcept
        : m_storage{std::move(other.m_storage)}
    {
    }

    auto operator=(log_t &&other) noexcept -> log_t &
    {
        if (this != &other)
        {
            using std::swap;
            swap(*this, other);
        }
        return *this;
    }

    log_t(const log_t &) noexcept = delete;
    auto operator=(const log_t &) noexcept -> log_t & = delete;

    ~log_t() noexcept = default;

    [[nodiscard]] auto append(std::string entry) noexcept -> bool
    {
        return m_storage.append(std::move(entry));
    }

    [[nodiscard]] auto append(std::string command, std::string key, std::string value) noexcept
        -> bool
    {
        return m_storage.append(std::move(command), std::move(key), std::move(value));
    }

    [[nodiscard]] auto read(std::size_t index) const noexcept -> std::optional<std::string>
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

  private:
    storage::log_storage_wrapper_t m_storage;
};
static_assert(TLogConcept<log_t>, "log_t should satisfy TLogConcept");

class log_builder_t
{
  public:
    log_builder_t() = default;

    [[nodiscard]] auto build(storage::log_storage_wrapper_t storage) const -> std::optional<log_t>
    {
        return log_t{std::move(storage)};
    }

  private:
    fs::path_t  m_file_path;
    std::string m_url;
};

} // namespace wal::log
