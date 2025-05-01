#pragma once

#include <sstream>

#include <absl/strings/ascii.h>
#include <spdlog/spdlog.h>

#include "backend.h"
#include "../../concepts.h"
#include "fs/append_only_file.h"

namespace wal::log::storage::backend
{

class append_only_file_storage_backend_t
    : public storage_backend_base_t<append_only_file_storage_backend_t>
{
  public:
    explicit append_only_file_storage_backend_t(fs::append_only_file_t &&file)
        : m_file(std::move(file))
    {
    }

    append_only_file_storage_backend_t(append_only_file_storage_backend_t &&other) noexcept
        : storage_backend_base_t<append_only_file_storage_backend_t>{std::move(other)},
          m_file{std::move(other.m_file)}
    {
    }

    auto operator=(append_only_file_storage_backend_t &&other) noexcept
        -> append_only_file_storage_backend_t &
    {
        if (this != &other)
        {
            m_file = std::move(other.m_file);
        }
        return *this;
    }

    append_only_file_storage_backend_t(const append_only_file_storage_backend_t &other) = delete;
    auto operator=(const append_only_file_storage_backend_t &) noexcept
        -> append_only_file_storage_backend_t & = delete;

    ~append_only_file_storage_backend_t() = default;

    friend class storage_backend_base_t;

  private:
    // TODO(lnikon): Use StrongTypes for adjacent parameters with similar type
    [[nodiscard]] auto write_impl(const char *data, ssize_t offset, std::size_t size) -> bool
    {
        (void)offset;
        return m_file.append({data, size})
            .transform([](ssize_t res) { return res >= 0; })
            .value_or(false);
    }

    [[nodiscard]] auto read_impl(ssize_t offset, std::size_t size) -> std::string
    {
        std::string buffer(size, '\0');
        const auto  res = m_file.read(offset, buffer.data(), size);
        if (!res.has_value())
        {
            spdlog::error("Failed to read from file storage. Offset={}, size={}", offset, size);
            return {};
        }

        buffer.resize(res.value());
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

    // TODO(lnikon): Would be a better strategy to create entirely new file with the truncated log
    // and then replace it with the existing one
    [[nodiscard]] auto reset_last_n_impl(std::size_t n) noexcept -> bool
    {
        std::size_t currentSize{size_impl()};
        if (currentSize == 0)
        {
            spdlog::warn("reset_last_n: File is empty.");
            return true;
        }

        // Read the entire file content.
        std::string data{read_impl(0, currentSize)};
        if (data.empty())
        {
            spdlog::error("reset_last_n: Unable to read data from file");
            return false;
        }

        // Split the data into log entries.
        std::istringstream       iss(data);
        std::vector<std::string> entries;
        std::string              line;
        while (std::getline(iss, line))
        {
            // Skip empty lines.
            if (!absl::StripAsciiWhitespace(line).empty())
            {
                entries.emplace_back(std::move(line));
            }
        }

        // Safety check.
        if (n > entries.size())
        {
            spdlog::error(
                "reset_last_n: Requested removal of {} entries exceeds existing {} entries.",
                n,
                entries.size());
            return false;
        }

        // Remove the last n entries.
        entries.resize(n);

        // Reconstruct the file content.
        std::string newContent;
        for (const auto &entry : entries)
        {
            newContent.append(entry);
            newContent.push_back('\n');
        }

        // Reset the underlying file.
        if (!reset_impl())
        {
            spdlog::error("reset_last_n: Unable to reset the file.");
            return false;
        }

        // Write back the truncated file.
        return write_impl(newContent.c_str(), 0, newContent.size());
    }

    fs::append_only_file_t m_file;
};
static_assert(TStorageBackendConcept<append_only_file_storage_backend_t>,
              "append_only_file_storage_backend_t must satisfy TStorageBackendConcept");

class file_storage_backend_builder_t final
    : public storage_backend_builder_t<append_only_file_storage_backend_t>
{
  public:
    using base_t = storage_backend_builder_t<append_only_file_storage_backend_t>;

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

    [[nodiscard]] auto build_impl() -> std::expected<append_only_file_storage_backend_t,
                                                     storage_backend_builder_error_t> override
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
        return append_only_file_storage_backend_t(std::move(file.value()));
    }
};

static_assert(TStorageBackendConcept<append_only_file_storage_backend_t>,
              "file_storage_backend_t must satisfy TStorageBackendConcept");

} // namespace wal::log::storage::backend
