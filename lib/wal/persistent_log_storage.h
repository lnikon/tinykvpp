#pragma once

#include <spdlog/spdlog.h>

#include "wal/concepts.h"
#include "fs/append_only_file.h"

#include <utility>
#include <expected>

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

/**
 * @brief A CRTP base class for storage backend. Implements TStorageBackendConcept
 * @tparam Derived A CRTP derived class
 */
template <typename Derived> class storage_backend_base_t
{
  public:
    friend Derived;

    [[nodiscard]] auto write(const std::byte *data, std::size_t offset, std::size_t size) -> bool
    {
        return static_cast<Derived *>(this)->write_impl(data, offset, size);
    }

    [[nodiscard]] auto read(std::size_t offset, std::size_t size) -> std::vector<std::byte>
    {
        return static_cast<Derived *>(this)->read_impl(offset, size);
    }

    [[nodiscard]] auto size() const -> std::size_t
    {
        return static_cast<Derived *>(this)->size_impl();
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
    explicit storage_backend_builder_t(storage_backend_config_t config)
        : m_config(std::move(config))
    {
    }

    storage_backend_builder_t(const storage_backend_builder_t &) = delete;
    auto operator=(const storage_backend_builder_t &) -> storage_backend_builder_t && = delete;

    storage_backend_builder_t(storage_backend_builder_t &&) = delete;
    auto operator=(storage_backend_builder_t &&) -> storage_backend_builder_t && = delete;

    virtual ~storage_backend_builder_t() = default;

    [[nodiscard]] auto build() -> std::expected<TBackendStorage, storage_backend_builder_error_t>
    {
        if (auto res = validate_config_impl(); res.has_value())
        {
            return res;
        }
        return build_impl();
    }

    [[nodiscard]] auto config() const
    {
        return m_config;
    }

  private:
    virtual auto validate_config_impl() -> std::optional<storage_backend_builder_error_t> = 0;
    virtual auto build_impl() -> std::expected<TBackendStorage, storage_backend_builder_error_t> = 0;

    storage_backend_config_t m_config;
};

class file_storage_backend_t : public storage_backend_base_t<file_storage_backend_t>
{
  public:
    explicit file_storage_backend_t(fs::append_only_file_t &&file)
        : m_file(std::move(file))
    {
    }

    friend class storage_backend_base_t;

  private:
    [[nodiscard]] auto write_impl(const std::byte *data, std::size_t offset, std::size_t size) -> bool
    {
        throw std::logic_error("Not implemented");
    }

    [[nodiscard]] auto read_impl(std::size_t offset, std::size_t size) -> std::vector<std::byte>
    {
        throw std::logic_error("Not implemented");
    }

    [[nodiscard]] auto size_impl() const -> std::size_t
    {
        throw std::logic_error("Not implemented");
    }

    [[nodiscard]] auto reset_impl() -> bool
    {
        throw std::logic_error("Not implemented");
    }

    fs::append_only_file_t m_file;
};

class file_storage_backend_builder_t final : public storage_backend_builder_t<file_storage_backend_t>
{
  public:
    using base_t = storage_backend_builder_t<file_storage_backend_t>;

    explicit file_storage_backend_builder_t(storage_backend_config_t config)
        : base_t(std::move(config))
    {
    }

  private:
    [[nodiscard]] auto validate_config_impl() -> std::optional<storage_backend_builder_error_t> override
    {
        if (config().file_path.empty())
        {
            return storage_backend_builder_error_t::kEmptyFileName;
        }

        return std::nullopt;
    }

    [[nodiscard]] auto build_impl() -> std::expected<file_storage_backend_t, storage_backend_builder_error_t> override
    {
        fs::append_only_file_t file(config().file_path);
        if (!file.is_open())
        {
            return std::unexpected(storage_backend_builder_error_t::kUnableToOpenFile);
        }
        return file_storage_backend_t(std::move(file));
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
    // else if constexpr (std::is_same_v<TStorageBackend, object_storage_backend_t>)
    // {
    //
    // }
    else
    {
        static_assert(false, "not supported backend storage type passed");
    }
}

// class object_store_storage_backend_t : public storage_backend_base_t<object_store_storage_backend_t>
// {
//   public:
//     explicit object_store_storage_backend_t()
//     {
//     }
//
//     friend class storage_backend_base_t<object_store_storage_backend_t>;
//
//   private:
//     bool write_impl(const std::byte *data, std::size_t offset, std::size_t size)
//     {
//         throw std::logic_error("Not implemented");
//     }
//
//     std::vector<std::byte> read_impl(std::size_t offset, std::size_t size)
//     {
//         throw std::logic_error("Not implemented");
//     }
//
//     std::size_t size_impl() const
//     {
//         throw std::logic_error("Not implemented");
//     }
//
//     bool reset_impl()
//     {
//         throw std::logic_error("Not implemented");
//     }
// };
//
// static_assert(TStorageBackendConcept<object_store_storage_backend_t>,
//               "file_storage_backend_t must satisfy TStorageBackendConcept");

template <TStorageBackendConcept TBackendStorage> class persistent_log_storage_t
{
  private:
    explicit persistent_log_storage_t(TBackendStorage &&backendStorage)
        : m_backendStorage(std::move(backendStorage))
    {
    }

  public:
    persistent_log_storage_t() = delete;

    void append(std::string entry)
    {
        if (!m_backendStorage.write(entry.data(), m_backendStorage.size(), entry.size()))
        {
            spdlog::error("Write failed in PersistentLogStorage\n");
        }
        m_inMemoryLog.emplace_back(std::move(entry));
    }

    [[nodiscard]] auto read(const size_t index) const -> std::optional<std::string>
    {
        return index < m_inMemoryLog.size() ? std::make_optional(m_inMemoryLog[index]) : std::nullopt;
    }

    [[nodiscard]] auto reset() -> bool
    {
        m_inMemoryLog.clear();
        m_backendStorage.reset();
        return false;
    }

  private:
    TBackendStorage          m_backendStorage;
    std::vector<std::string> m_inMemoryLog;
};

static_assert(TLogStorageConcept<persistent_log_storage_t<file_storage_backend_t>>,
              "persistent_log_storage_t must satisfy TLogStorageConcept");

// static_assert(TLogStorageConcept<persistent_log_storage_t<object_store_storage_backend_t>>,
//               "object_store_storage_backend_t must satisfy TLogStorageConcept");

// --------------------------------------------------------
// Builder for persistent_log_storage_t (instance-based).
// --------------------------------------------------------
