#pragma once

#include <optional>

#include <libassert/assert.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "../concepts.h"
#include "persistent_log_storage.h"
#include "raft/raft.h"

namespace wal::log::storage
{

template <typename TEntry> class replicated_log_storage_impl_t
{
  public:
    using entry_type = TEntry;

    using file_log_t =
        persistent_log_storage_t<backend::append_only_file_storage_backend_t, TEntry>;

    replicated_log_storage_impl_t() = delete;

    replicated_log_storage_impl_t(
        file_log_t log, std::shared_ptr<raft::consensus_module_t> pConsensusModule
    ) noexcept;

    replicated_log_storage_impl_t(replicated_log_storage_impl_t &&other) noexcept;

    auto operator=(replicated_log_storage_impl_t &&other) noexcept
        -> replicated_log_storage_impl_t &;

    replicated_log_storage_impl_t(const replicated_log_storage_impl_t &) = delete;
    auto operator=(const replicated_log_storage_impl_t &) = delete;

    ~replicated_log_storage_impl_t() noexcept = default;

    [[nodiscard]] auto append(entry_type entry) noexcept -> bool;
    [[nodiscard]] auto read(size_t index) const -> std::optional<entry_type>;
    [[nodiscard]] auto reset() -> bool;
    [[nodiscard]] auto size() const -> std::size_t;
    [[nodiscard]] auto reset_last_n(std::size_t n) -> bool
    {
        const auto logSize{m_log.size()};
        if (logSize < n)
        {
            spdlog::error(
                "in_memory_log_storage_impl_t::reset_last_n: Log size {} is smaller "
                "than provided {} removal size",
                logSize,
                n
            );
            return false;
        }
        m_log.reset_last_n(logSize - n);
        return true;
    }

  private:
    file_log_t                                m_log;
    std::shared_ptr<raft::consensus_module_t> m_pConsensusModule{nullptr};
};

class replicated_log_storage_builder_t final
{
  public:
    template <typename TEntry>
    [[nodiscard]] auto build(
        replicated_log_storage_impl_t<TEntry>::file_log_t log,
        std::shared_ptr<raft::consensus_module_t>         pConsensusModule
    ) noexcept -> std::optional<replicated_log_storage_impl_t<TEntry>>
    {
        return std::make_optional(
            replicated_log_storage_impl_t{std::move(log), std::move(pConsensusModule)}
        );
    }
};

template <typename TEntry>
replicated_log_storage_impl_t<TEntry>::replicated_log_storage_impl_t(
    file_log_t log, std::shared_ptr<raft::consensus_module_t> pConsensusModule
) noexcept
    : m_log{std::move(log)},
      m_pConsensusModule{std::move(pConsensusModule)}
{
}

template <typename TEntry>
replicated_log_storage_impl_t<TEntry>::replicated_log_storage_impl_t(
    replicated_log_storage_impl_t &&other
) noexcept
    : m_log{std::move(other.m_log)},
      m_pConsensusModule{std::move(other.m_pConsensusModule)}
{
    other.m_pConsensusModule = nullptr;
}

template <typename TEntry>
auto replicated_log_storage_impl_t<TEntry>::operator=(
    replicated_log_storage_impl_t<TEntry> &&other
) noexcept -> replicated_log_storage_impl_t &
{
    if (this == &other)
    {
        return *this;
    }

    m_log = std::move(other.m_log);

    m_pConsensusModule = std::move(other.m_pConsensusModule);
    other.m_pConsensusModule = nullptr;

    return *this;
}

template <typename TEntry>
[[nodiscard]] auto replicated_log_storage_impl_t<TEntry>::append(entry_type entry) noexcept -> bool
{
    // Serialize the entry into an array of bytes, as the actual entry type is irrelevant for the
    // Raft.
    std::stringstream entryStream;
    entryStream << entry;

    spdlog::info("replicated_log_storage_impl_t::append: Going append entry={}", entryStream.str());

    // // Replicate first, if it succeeds, then update the WAL.
    // if (m_pConsensusModule->replicate(entryStream.str()) !=
    //     raft::raft_operation_status_k::success_k)
    // {
    //     spdlog::error(
    //         "replicated_log_storage_t::append: Unable to replicate the log entry. entry={}",
    //         entryStream.str()
    //     );
    //     return false;
    // }

    // This means that log got successfully replicated, time to update the WAL.
    if (!m_log.append(std::move(entry)))
    {
        spdlog::error("replicated_log_storage_t::append: Unable to the WAL");
        return false;
    }

    return true;
}

template <typename TEntry>
[[nodiscard]] auto replicated_log_storage_impl_t<TEntry>::read(size_t index) const
    -> std::optional<entry_type>
{
    return m_log.read(index);
}

template <typename TEntry> [[nodiscard]] auto replicated_log_storage_impl_t<TEntry>::reset() -> bool
{
    return m_log.reset();
}

template <typename TEntry>
[[nodiscard]] auto replicated_log_storage_impl_t<TEntry>::size() const -> std::size_t
{
    return m_log.size();
}

template <typename TEntry>
    requires TLogStorageConcept<replicated_log_storage_impl_t, TEntry>
using replicated_log_storage_t = replicated_log_storage_impl_t<TEntry>;

} // namespace wal::log::storage
