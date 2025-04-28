#pragma once

#include <utility>
#include <expected>

#include <spdlog/spdlog.h>
#include <absl/strings/ascii.h>

#include "../concepts.h"
#include "backend/backend.h"
#include "backend/file_storage_backend.h"

namespace wal::log
{

template <TStorageBackendConcept TStorageBackend>
auto create_storage_backend_builder(storage::backend::storage_backend_config_t config)
    -> std::unique_ptr<storage::backend::storage_backend_builder_t<TStorageBackend>>
{
    if constexpr (std::is_same_v<TStorageBackend, storage::backend::file_storage_backend_t>)
    {
        return std::make_unique<storage::backend::file_storage_backend_builder_t>(
            std::move(config));
    }
    else
    {
        static_assert(false, "not supported backend storage type passed");
    }
}
template <TStorageBackendConcept TBackendStorage> class persistent_log_storage_builder_t;

template <TStorageBackendConcept TBackendStorage> class persistent_log_storage_t
{
  private:
    explicit persistent_log_storage_t(TBackendStorage &&backendStorage)
        : m_backendStorage(std::move(backendStorage))
    {
        const std::string  raw = m_backendStorage.read(0, m_backendStorage.size());
        std::istringstream stream(raw);
        for (std::string line; std::getline(stream, line);)
        {
            if (absl::StripAsciiWhitespace(line).empty())
            {
                continue;
            }
            m_inMemoryLog.emplace_back(std::move(line));
        }
    }

  public:
    persistent_log_storage_t() = delete;

    persistent_log_storage_t(persistent_log_storage_t &&other) noexcept
        : m_backendStorage{std::move(other.m_backendStorage)},
          m_inMemoryLog{std::move(other.m_inMemoryLog)}
    {
    }

    auto operator=(persistent_log_storage_t &&other) noexcept -> persistent_log_storage_t &
    {
        if (this != &other)
        {
            using std::swap;
            swap(*this, other);
        }
        return *this;
    }

    persistent_log_storage_t(const persistent_log_storage_t &) = delete;
    auto operator=(const persistent_log_storage_t &) = delete;

    ~persistent_log_storage_t() noexcept = default;

    [[nodiscard]] auto append(std::string entry) -> bool
    {
        if (!m_backendStorage.write(
                static_cast<const char *>(entry.data()), m_backendStorage.size(), entry.size()))
        {
            spdlog::error(
                "Persistent log storage write failed. Entry={}, size={}\n", entry, entry.size());
            return false;
        }
        m_inMemoryLog.emplace_back(std::move(entry));
        return true;
    }

    [[nodiscard]] auto append(std::string command, std::string key, std::string value) -> bool
    {
        return append(fmt::format("{} {} {}", command, key, value));
    }

    [[nodiscard]] auto read(const size_t index) const -> std::optional<std::string>
    {
        if (index < m_inMemoryLog.size())
        {
            return std::make_optional(m_inMemoryLog[index]);
        }

        return std::nullopt;
    }

    [[nodiscard]] auto reset() -> bool
    {
        m_inMemoryLog.clear();
        return m_backendStorage.reset();
    }

    [[nodiscard]] auto size() const -> std::size_t
    {
        return m_backendStorage.size();
    }

    friend class persistent_log_storage_builder_t<TBackendStorage>;

  private:
    TBackendStorage          m_backendStorage;
    std::vector<std::string> m_inMemoryLog;
};

static_assert(
    TLogStorageConcept<persistent_log_storage_t<storage::backend::file_storage_backend_t>>,
    "persistent_log_storage_t must satisfy TLogStorageConcept");

enum class persistent_log_storage_builder_error_t : std::uint8_t
{
    kBackendBuildFailed,
};

/**
 * @brief Builder for persistent_log_storage_t with different backend types
 * @tparam TBackendStorage The backend storage type that satisfies
 * TStorageBackendConcept
 */
template <TStorageBackendConcept TBackendStorage> class persistent_log_storage_builder_t
{
  public:
    explicit persistent_log_storage_builder_t(storage::backend::storage_backend_config_t config)
        : m_config(std::move(config))
    {
    }

    persistent_log_storage_builder_t(const persistent_log_storage_builder_t &) = delete;
    auto operator=(const persistent_log_storage_builder_t &)
        -> persistent_log_storage_builder_t & = delete;

    persistent_log_storage_builder_t(persistent_log_storage_builder_t &&) noexcept = default;
    auto operator=(persistent_log_storage_builder_t &&) noexcept
        -> persistent_log_storage_builder_t & = default;

    ~persistent_log_storage_builder_t() = default;

    /**
     * @brief Build the persistent_log_storage_t with the configured backend
     * @return Expected containing the built storage or an error
     */
    [[nodiscard]] auto build() -> std::expected<persistent_log_storage_t<TBackendStorage>,
                                                persistent_log_storage_builder_error_t>
    {
        // Create the appropriate backend builder
        auto backend_builder = create_storage_backend_builder<TBackendStorage>(m_config);

        // Build the backend
        auto backend_result = backend_builder->build();
        if (!backend_result)
        {
            spdlog::error("Failed to build backend storage: {}",
                          static_cast<int>(backend_result.error()));
            return std::unexpected(persistent_log_storage_builder_error_t::kBackendBuildFailed);
        }

        // Create the persistent log storage with the built backend
        return persistent_log_storage_t<TBackendStorage>(std::move(backend_result.value()));
    }

    // Accessor for the config
    [[nodiscard]] auto config() const -> const storage::backend::storage_backend_config_t &
    {
        return m_config;
    }

  private:
    storage::backend::storage_backend_config_t m_config;
};

/**
 * @brief Factory function to create a persistent log storage builder
 * @tparam TBackendStorage The backend storage type
 * @param config Configuration for the storage backend
 * @return A builder for persistent_log_storage_t with the specified backend
 */
template <TStorageBackendConcept TBackendStorage>
auto create_persistent_log_storage_builder(storage::backend::storage_backend_config_t config)
    -> persistent_log_storage_builder_t<TBackendStorage>
{
    return persistent_log_storage_builder_t<TBackendStorage>(std::move(config));
}

} // namespace wal::log
