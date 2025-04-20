#pragma once

#include <spdlog/spdlog.h>

#include <absl/strings/ascii.h>

#include <utility>
#include <expected>

#include "concepts.h"
#include "fs/append_only_file.h"
#include "fs/types.h"

namespace wal::log
{

enum class storage_backend_builder_error_t : std::uint8_t
{
    kEmptyFileName,
    kUnableToOpenFile,
    kWrongFileFormat,
};

enum class storage_backend_error_t : std::uint8_t
{
    kSuccess,
    kWriteFailed,
    kReadFailed,
    kSizeFailed,
    kResetFailed
};

struct storage_backend_config_t
{
    fs::path_t file_path;
};

template <typename Derived> class storage_backend_base_t
{
  public:
    friend Derived;

    [[nodiscard]] auto write(const char *data, std::size_t offset, std::size_t size) -> bool
    {
        return static_cast<Derived *>(this)->write_impl(data, offset, size);
    }

    [[nodiscard]] auto read(std::size_t offset, std::size_t size) -> std::string
    {
        return static_cast<Derived *>(this)->read_impl(offset, size);
    }

    [[nodiscard]] auto size() const -> std::size_t
    {
        return static_cast<const Derived *>(this)->size_impl();
    }

    [[nodiscard]] auto reset() -> bool
    {
        return static_cast<Derived *>(this)->reset_impl();
    }

  private:
    storage_backend_base_t() = default;
};

template <TStorageBackendConcept TBackendStorage> class storage_backend_builder_t
{
  public:
    explicit storage_backend_builder_t(storage_backend_config_t config);

    storage_backend_builder_t(const storage_backend_builder_t &) = delete;
    auto operator=(const storage_backend_builder_t &) -> storage_backend_builder_t && = delete;

    storage_backend_builder_t(storage_backend_builder_t &&) = delete;
    auto operator=(storage_backend_builder_t &&) -> storage_backend_builder_t && = delete;

    virtual ~storage_backend_builder_t() = default;

    [[nodiscard]] auto build() -> std::expected<TBackendStorage, storage_backend_builder_error_t>
    {
        if (auto res = validate_config_impl(); res.has_value())
        {
            return std::unexpected(res.value());
        }
        return build_impl();
    }

    [[nodiscard]] auto config() const
    {
        return m_config;
    }

  private:
    virtual auto validate_config_impl() -> std::optional<storage_backend_builder_error_t> = 0;
    virtual auto build_impl()
        -> std::expected<TBackendStorage, storage_backend_builder_error_t> = 0;

    storage_backend_config_t m_config;
};

template <TStorageBackendConcept TBackendStorage>
storage_backend_builder_t<TBackendStorage>::storage_backend_builder_t(
    storage_backend_config_t config)
    : m_config(std::move(config))
{
}

class file_storage_backend_t : public storage_backend_base_t<file_storage_backend_t>
{
  public:
    explicit file_storage_backend_t(fs::append_only_file_t &&file)
        : m_file(std::move(file))
    {
    }

    file_storage_backend_t(file_storage_backend_t &&other) noexcept
        : storage_backend_base_t<file_storage_backend_t>{std::move(other)},
          m_file{std::move(other.m_file)}
    {
    }

    auto operator=(file_storage_backend_t &&other) noexcept -> file_storage_backend_t &
    {
        if (this != &other)
        {
            m_file = std::move(other.m_file);
        }
        return *this;
    }

    file_storage_backend_t(const file_storage_backend_t &other) = delete;
    auto operator=(const file_storage_backend_t &) noexcept -> file_storage_backend_t & = delete;

    ~file_storage_backend_t() = default;

    friend class storage_backend_base_t;

  private:
    // TODO(lnikon): Use StrongTypes for adjacent parameters with similar type
    [[nodiscard]] auto write_impl(const char *data, std::size_t offset, std::size_t size) -> bool
    {
        return m_file.append({data, size})
            .transform([](ssize_t res) { return res >= 0; })
            .value_or(false);
    }

    [[nodiscard]] auto read_impl(std::size_t offset, std::size_t size) -> std::string
    {
        std::string buffer;
        buffer.resize(size);
        if (const auto res = m_file.read(offset, buffer.data(), size); !res.has_value())
        {
            spdlog::error("Failed to read from file storage. Offset={}, size={}", offset, size);
            return {};
        }
        return buffer;
    }

    [[nodiscard]] auto size_impl() const -> std::size_t
    {
        return m_file.size().value_or(0);
    }

    [[nodiscard]] auto reset_impl() -> bool
    {
        return m_file.reset().has_value();
    }

    fs::append_only_file_t m_file;
};

class file_storage_backend_builder_t final
    : public storage_backend_builder_t<file_storage_backend_t>
{
  public:
    using base_t = storage_backend_builder_t<file_storage_backend_t>;

    explicit file_storage_backend_builder_t(storage_backend_config_t config)
        : base_t(std::move(config))
    {
    }

  private:
    [[nodiscard]] auto validate_config_impl()
        -> std::optional<storage_backend_builder_error_t> override
    {
        if (config().file_path.empty())
        {
            return storage_backend_builder_error_t::kEmptyFileName;
        }

        return std::nullopt;
    }

    [[nodiscard]] auto build_impl()
        -> std::expected<file_storage_backend_t, storage_backend_builder_error_t> override
    {
        auto file = fs::append_only_file_builder_t{}.build(config().file_path.c_str(), true);
        if (!file)
        {
            const auto &error{file.error()};
            switch (error.code)
            {
            default:
                return std::unexpected(storage_backend_builder_error_t::kUnableToOpenFile);
            }
        }
        return file_storage_backend_t(std::move(file.value()));
    }
};

static_assert(TStorageBackendConcept<file_storage_backend_t>,
              "file_storage_backend_t must satisfy TStorageBackendConcept");

template <TStorageBackendConcept TStorageBackend>
auto create_storage_backend_builder(storage_backend_config_t config)
    -> std::unique_ptr<storage_backend_builder_t<TStorageBackend>>
{
    if constexpr (std::is_same_v<TStorageBackend, file_storage_backend_t>)
    {
        return std::make_unique<file_storage_backend_builder_t>(std::move(config));
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
        using std::swap;
        swap(*this, other);
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

static_assert(TLogStorageConcept<persistent_log_storage_t<file_storage_backend_t>>,
              "persistent_log_storage_t must satisfy TLogStorageConcept");

// --------------------------------------------------------
// Builder for persistent_log_storage_t (instance-based).
// --------------------------------------------------------

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
    explicit persistent_log_storage_builder_t(storage_backend_config_t config)
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
    [[nodiscard]] auto config() const -> const storage_backend_config_t &
    {
        return m_config;
    }

  private:
    storage_backend_config_t m_config;
};

/**
 * @brief Factory function to create a persistent log storage builder
 * @tparam TBackendStorage The backend storage type
 * @param config Configuration for the storage backend
 * @return A builder for persistent_log_storage_t with the specified backend
 */
template <TStorageBackendConcept TBackendStorage>
auto create_persistent_log_storage_builder(storage_backend_config_t config)
    -> persistent_log_storage_builder_t<TBackendStorage>
{
    return persistent_log_storage_builder_t<TBackendStorage>(std::move(config));
}

} // namespace wal::log
