#pragma once

#include <spdlog/spdlog.h>

#include "backend.h"
#include "../../concepts.h"
#include "fs/append_only_file.h"

namespace wal::log::storage::backend
{

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
        (void)offset;
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

} // namespace wal::log::storage::backend
