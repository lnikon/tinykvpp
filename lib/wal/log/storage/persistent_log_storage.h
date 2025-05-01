#pragma once

#include <utility>
#include <expected>

#include <spdlog/spdlog.h>
#include <absl/strings/ascii.h>

#include "../concepts.h"
#include "backend/backend.h"

#include "backend/append_only_file_storage_backend.h"

namespace wal::log::storage
{

template <TStorageBackendConcept TStorageBackend>
auto create_storage_backend_builder(backend::storage_backend_config_t config) noexcept
    -> std::unique_ptr<backend::storage_backend_builder_t<TStorageBackend>>
{
    if constexpr (std::is_same_v<TStorageBackend, backend::append_only_file_storage_backend_t>)
    {
        return std::make_unique<backend::file_storage_backend_builder_t>(std::move(config));
    }
    else
    {
        static_assert(false, "not supported backend storage type passed");
    }
    return nullptr;
}
template <TStorageBackendConcept TBackendStorage, typename TEntry>
class persistent_log_storage_builder_t;

template <TStorageBackendConcept TBackendStorage, typename TEntry>
class persistent_log_storage_impl_t
{
    explicit persistent_log_storage_impl_t(TBackendStorage &&backendStorage)
        : m_backendStorage(std::move(backendStorage))
    {
        // TODO(lnikon): Move recovery into builder
        recover();
    }

  public:
    using entry_type = TEntry;

    persistent_log_storage_impl_t() = delete;

    persistent_log_storage_impl_t(persistent_log_storage_impl_t &&other) noexcept
        : m_backendStorage{std::move(other.m_backendStorage)},
          m_inMemoryLog{std::move(other.m_inMemoryLog)}
    {
    }

    auto operator=(persistent_log_storage_impl_t &&other) noexcept
        -> persistent_log_storage_impl_t &
    {
        if (this != &other)
        {
            using std::swap;
            swap(*this, other);
        }
        return *this;
    }

    persistent_log_storage_impl_t(const persistent_log_storage_impl_t &) = delete;
    auto operator=(const persistent_log_storage_impl_t &) = delete;

    ~persistent_log_storage_impl_t() noexcept = default;

    [[nodiscard]] auto append(entry_type entry) -> bool
    {
        std::stringstream ss;
        ss << entry << '\n';
        if (auto stringEntry = ss.str();
            !m_backendStorage.write(static_cast<const char *>(stringEntry.data()),
                                    m_backendStorage.size(),
                                    stringEntry.size()))
        {
            spdlog::error("Persistent log storage write failed. Entry={}, size={}\n",
                          stringEntry,
                          stringEntry.size());
            return false;
        }
        m_inMemoryLog.emplace_back(std::move(entry));
        return true;
    }

    [[nodiscard]] auto read(const size_t index) const -> std::optional<entry_type>
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
        return m_inMemoryLog.size();
    }

    [[nodiscard]] auto reset_last_n(std::size_t n) -> bool
    {
        const auto inMemoryLogSize{m_inMemoryLog.size()};
        if (inMemoryLogSize < n)
        {
            spdlog::error("persistent_log_storage_impl_t::reset_last_n: Log size {} is smaller "
                          "than provided {} removal size",
                          inMemoryLogSize,
                          n);
            return false;
        }

        if (m_backendStorage.reset_last_n())
        {
            spdlog::error(
                "persistent_log_storage_impl_t::reset_last_n: Failed to reset last {} entries", n);
            return false;
        }

        m_inMemoryLog.resize(inMemoryLogSize - n);

        return true;
    }

    friend class persistent_log_storage_builder_t<TBackendStorage, entry_type>;

  private:
    void recover() noexcept
    {
        const std::string raw = m_backendStorage.read(0, m_backendStorage.size());
        if (!raw.empty())
        {
            std::istringstream stream(raw);
            std::string        line;
            std::size_t        lineNumber{0};
            while (std::getline(stream, line))
            {
                if (absl::StripAsciiWhitespace(line).empty())
                {
                    spdlog::warn("persistent_log_storage_impl_t: Empty log line at {}", lineNumber);
                    continue;
                }

                std::stringstream ss{line};

                TEntry entry;
                ss >> entry;

                m_inMemoryLog.emplace_back(std::move(entry));

                lineNumber++;
            }
        }
        else
        {
            spdlog::info("persistent_log_storage_impl_t: Nothing to recover");
        }

        // TODO(lnikon): 'recover()' will be called from a builder
        // return true;
    }

    TBackendStorage         m_backendStorage;
    std::vector<entry_type> m_inMemoryLog;
};

template <TStorageBackendConcept TBackend, typename TEntry>
    requires TLogStorageConcept<persistent_log_storage_impl_t, TBackend, TEntry>
using persistent_log_storage_t = persistent_log_storage_impl_t<TBackend, TEntry>;

enum class persistent_log_storage_builder_error_t : std::uint8_t
{
    kBackendBuildFailed,
};

/**
 * @brief Builder for persistent_log_storage_t with different backend types
 * @tparam TBackendStorage The backend storage type that satisfies
 * TStorageBackendConcept
 */
template <TStorageBackendConcept TBackendStorage, typename TEntry>
class persistent_log_storage_builder_t
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
    [[nodiscard]] auto build() -> std::expected<persistent_log_storage_t<TBackendStorage, TEntry>,
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
        return persistent_log_storage_t<TBackendStorage, TEntry>(std::move(backend_result.value()));
    }

    // Accessor for the config
    [[nodiscard]] auto config() const -> const backend::storage_backend_config_t &
    {
        return m_config;
    }

  private:
    backend::storage_backend_config_t m_config;
};

} // namespace wal::log::storage
