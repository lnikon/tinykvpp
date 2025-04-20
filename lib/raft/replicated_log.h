#pragma once

#include <libassert/assert.hpp>
#include <fmt/format.h>

#include "log/log.h"
#include "raft/raft.h"

namespace wal::log
{

class replicated_log_t
{
  public:
    replicated_log_t() = delete;

    replicated_log_t(wal::log::log_variant_t                   log,
                     std::shared_ptr<raft::consensus_module_t> pConsensusModule) noexcept
        : m_log{std::move(log)},
          m_pConsensusModule{std::move(pConsensusModule)}
    {
    }

    replicated_log_t(replicated_log_t &&other) noexcept
        : m_log{std::move(other.m_log)},
          m_pConsensusModule{std::move(other.m_pConsensusModule)}
    {
        other.m_pConsensusModule = nullptr;
    }

    auto operator=(replicated_log_t &&other) noexcept -> replicated_log_t &
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

    replicated_log_t(const replicated_log_t &) = delete;
    auto operator=(const replicated_log_t &) = delete;

    ~replicated_log_t() noexcept = default;

    [[nodiscard]] auto append(std::string entry) noexcept -> bool
    {
        return std::visit([&](auto &log) { return log.append(entry); }, m_log);
    }

    [[nodiscard]] auto append(std::string command, std::string key, std::string value) noexcept
        -> bool
    {
        if (!append(fmt::format("{} {} {}", command, key, value)))
        {
            return false;
        }

        LogEntry logEntry;
        logEntry.set_command(command);
        logEntry.set_key(key);
        logEntry.set_value(value);

        bool ok{false};
        ASSERT(ok = m_pConsensusModule->replicate(std::move(logEntry)),
               "failed to replicate entry");
        return ok;
    }

    [[nodiscard]] auto read(size_t index) const -> std::optional<std::string>
    {
        return std::visit([&](auto &log) { return log.read(index); }, m_log);
    }

    [[nodiscard]] auto reset() -> bool
    {
        return std::visit([&](auto &log) { return log.reset(); }, m_log);
    }

    [[nodiscard]] auto size() const -> std::size_t
    {
        return std::visit([&](auto &log) { return log.size(); }, m_log);
    }

  private:
    wal::log::log_variant_t                   m_log;
    std::shared_ptr<raft::consensus_module_t> m_pConsensusModule{nullptr};
};

static_assert(TLogStorageConcept<replicated_log_t>, "replicated_log_t must satisfy TLogConcept");

class replicated_log_builder_t final
{
  public:
    [[nodiscard]] auto build(wal::log::log_variant_t                   log,
                             std::shared_ptr<raft::consensus_module_t> pConsensusModule) noexcept
        -> replicated_log_t
    {
        return replicated_log_t{std::move(log), std::move(pConsensusModule)};
    }
};

} // namespace wal::log
