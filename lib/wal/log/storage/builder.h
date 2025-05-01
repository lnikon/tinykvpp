#pragma once

#include <optional>
#include <variant>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "../../common.h"
#include "in_memory_log_storage.h"
#include "persistent_log_storage.h"
#include "replicated_log_storage.h"
#include "backend/append_only_file_storage_backend.h"

namespace wal::log::storage
{

template <typename TEntry>
using log_storage_variant_t = std::variant<
    // In-memory log storage
    in_memory_log_storage_t<TEntry>,
    // Append-only file-based log storage
    persistent_log_storage_t<backend::append_only_file_storage_backend_t, TEntry>,
    // Append-only file-based replicated log storage
    replicated_log_storage_t<TEntry>>;

template <typename TEntry> class log_storage_wrapper_impl_t final
{
  public:
    using entry_type = TEntry;

    explicit log_storage_wrapper_impl_t(log_storage_variant_t<TEntry> storage)
        : m_storage{std::move(storage)}
    {
    }

    [[nodiscard]] auto append(entry_type entry) -> bool
    {
        return std::visit([&](auto &storage) { return storage.append(entry); }, m_storage);
    }

    [[nodiscard]] auto read(size_t index) const -> std::optional<entry_type>
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

    [[nodiscard]] auto reset_last_n(std::size_t n) -> bool
    {
        return m_storage.reset_last_n(n);
    }

  private:
    log_storage_variant_t<TEntry> m_storage;
};

template <typename TEntry>
    requires TLogStorageConcept<log_storage_wrapper_impl_t, TEntry>
using log_storage_wrapper_t = log_storage_wrapper_impl_t<TEntry>;

class log_storage_builder_t final
{
  public:
    auto set_file_path(fs::path_t path) -> log_storage_builder_t &
    {
        m_path = std::move(path);
        return *this;
    }

    auto set_check_path_exists(bool check) noexcept -> log_storage_builder_t &
    {
        m_check_path = check;
        return *this;
    }

    auto set_consensus_module(std::shared_ptr<raft::consensus_module_t> consensusModule)
        -> log_storage_builder_t &
    {
        m_pConsensusModule = consensusModule;
        return *this;
    }

    template <typename TEntry>
    [[nodiscard]] auto build(log_storage_type_k type) noexcept
        -> std::optional<log_storage_wrapper_t<TEntry>>
    {
        if (type == log_storage_type_k::in_memory_k)
        {
            return log_storage_wrapper_t<TEntry>{storage::in_memory_log_storage_t<TEntry>()};
        }

        if (type == log_storage_type_k::file_based_persistent_k)
        {
            if (!check_path())
            {
                return std::nullopt;
            }

            if (auto storage =
                    persistent_log_storage_builder_t<backend::append_only_file_storage_backend_t,
                                                     TEntry>{{.file_path = m_path.value()}}
                        .build();
                storage.has_value())
            {
                return std::make_optional(
                    log_storage_wrapper_t<TEntry>{std::move(storage.value())});
            }

            return std::nullopt;
        }

        if (type == log_storage_type_k::replicated_log_storage_k)
        {
            if (!check_path())
            {
                return std::nullopt;
            }

            if (!m_pConsensusModule)
            {
                spdlog::error("log_storage_builder_t: ConsensusModule not set");
                return std::nullopt;
            }

            if (auto fileBasedStorage =
                    persistent_log_storage_builder_t<backend::append_only_file_storage_backend_t,
                                                     TEntry>{{.file_path = m_path.value()}}
                        .build();
                fileBasedStorage.has_value())
            {
                if (auto replicatedStorage = replicated_log_storage_builder_t{}.build<TEntry>(
                        std::move(fileBasedStorage.value()), std::move(m_pConsensusModule));
                    replicatedStorage.has_value())
                {
                    return std::make_optional(
                        log_storage_wrapper_t<TEntry>{std::move(replicatedStorage.value())});
                }

                spdlog::error("log_storage_builder_t: Unable to build replicated log storage");
                return std::nullopt;
            }

            spdlog::error(
                "log_storage_builder_t: Unable to build persistent file-based log storage");
            return std::nullopt;
        }

        return std::nullopt;
    }

  private:
    [[nodiscard]] auto check_path() const noexcept -> bool
    {
        if (!m_path.has_value())
        {
            spdlog::error("log_storage_builder_t: WAL path not set");
            return false;
        }

        if (m_path.value().empty())
        {
            spdlog::error("log_storage_builder_t: WAL path is empty");
            return false;
        }

        if (m_check_path && !exists(m_path.value()))
        {
            spdlog::error("WAL path does not exist: {}", m_path.value().c_str());
            return false;
        }

        return true;
    }

    bool                      m_check_path{true};
    std::optional<fs::path_t> m_path{std::nullopt};

    std::shared_ptr<raft::consensus_module_t> m_pConsensusModule{nullptr};
};

} // namespace wal::log::storage
