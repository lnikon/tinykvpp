#pragma once

#include <expected>

#include "../../concepts.h"
#include "fs/types.h"

namespace wal::log::storage::backend
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

    [[nodiscard]] auto write(const char *data, ssize_t offset, std::size_t size) -> bool
    {
        return static_cast<Derived *>(this)->write_impl(data, offset, size);
    }

    [[nodiscard]] auto read(ssize_t offset, std::size_t size) -> std::string
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
    auto operator=(const storage_backend_builder_t &) -> storage_backend_builder_t & = delete;

    storage_backend_builder_t(storage_backend_builder_t &&) = delete;
    auto operator=(storage_backend_builder_t &&) -> storage_backend_builder_t & = delete;

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

} // namespace wal::log::storage::backend
