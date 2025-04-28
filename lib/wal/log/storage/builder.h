#pragma once

#include "../../common.h"
#include "in_memory_log_storage.h"
#include "persistent_log_storage.h"
#include "backend/file_storage_backend.h"

#include <optional>
#include <filesystem>

namespace wal::log::storage
{

using log_storage_variant_t = std::variant<
    wal::log::storage::in_memory_log_storage_t,
    wal::log::persistent_log_storage_t<wal::log::storage::backend::file_storage_backend_t>>;

class log_storage_wrapper_t final
{
  public:
    explicit log_storage_wrapper_t(log_storage_variant_t storage)
        : m_storage{std::move(storage)}
    {
    }

    [[nodiscard]] auto append(std::string entry) -> bool
    {
        return std::visit([&](auto &storage) { return storage.append(entry); }, m_storage);
    }

    [[nodiscard]] auto append(std::string command, std::string key, std::string value) -> bool
    {
        return std::visit([&](auto &storage) { return storage.append(command, key, value); },
                          m_storage);
    }

    [[nodiscard]] auto read(size_t index) const -> std::optional<std::string>
    {
        return std::visit([&](auto &storage) { return storage.read(index); }, m_storage);
    }

    [[nodiscard]] auto reset() -> bool
    {
        return std::visit([&](auto &storage) { return storage.reset(); }, m_storage);
    }

    [[nodiscard]] auto size() const -> std::size_t
    {
        return std::visit([&](auto &storage) { return storage.size(); }, m_storage);
    }

  private:
    log_storage_variant_t m_storage;
};
static_assert(TLogStorageConcept<log_storage_wrapper_t>,
              "log_storage_wrapper_t must sutisfy TLogStorageConcept concept");

class log_storage_builder_t final
{
  public:
    auto set_file_path(fs::path_t path) -> log_storage_builder_t &
    {
        m_path = std::move(path);
        return *this;
    }

    [[nodiscard]] auto build(wal::log_storage_type_k type) noexcept
        -> std::optional<log_storage_wrapper_t>
    {
        if (type == wal::log_storage_type_k::in_memory_k)
        {
            return log_storage_wrapper_t{wal::log::storage::in_memory_log_storage_t()};
        }

        if (type == wal::log_storage_type_k::file_based_persistent_k)
        {
            if (!m_path.has_value() || m_path.value().empty())
            {
                return std::nullopt;
            }

            if (type == wal::log_storage_type_k::file_based_persistent_k)
            {
                if (!std::filesystem::exists(m_path.value()))
                {
                    spdlog::error("WAL path does not exist: {}", m_path.value().c_str());
                    return std::nullopt;
                }
            }

            if (auto storage =
                    persistent_log_storage_builder_t<
                        wal::log::storage::backend::file_storage_backend_t>{
                        {.file_path = m_path.value()}}
                        .build();
                storage.has_value())
            {
                return std::make_optional(log_storage_wrapper_t{std::move(storage.value())});
            }

            return std::nullopt;
        }

        return std::nullopt;
    }

  private:
    std::optional<fs::path_t> m_path{std::nullopt};
};

} // namespace wal::log::storage
