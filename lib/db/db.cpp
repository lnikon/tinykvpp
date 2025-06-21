#include "db.h"
#include "db/manifest/manifest.h"
#include "memtable.h"
#include "raft/raft.h"
#include "wal.h"

#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

namespace db
{

db_t::db_t(
    config::shared_ptr_t   config,
    wal_ptr_t              pWal,
    manifest::shared_ptr_t pManifest,
    lsmtree_ptr_t          pLsmtree
) noexcept
    : m_config{config},
      m_pWal{std::move(pWal)},
      m_pManifest{std::move(pManifest)},
      m_pLsmtree{std::move(pLsmtree)}
{
    assert(m_config);
    assert(m_pManifest);
    assert(m_pWal);
    assert(m_pLsmtree);
}

db_t::db_t(db_t &&other) noexcept
    : m_config{std::move(other.m_config)},
      m_pWal{std::move(other.m_pWal)},
      m_pManifest{std::move(other.m_pManifest)},
      m_pLsmtree{std::move(other.m_pLsmtree)}
{
}

auto db_t::operator=(db_t &&other) noexcept -> db_t &
{
    if (this != &other)
    {
        db_t temp{std::move(other)};
        swap(temp);
    }
    return *this;
}

auto db_t::open() -> bool
{
    return prepare_directory_structure();
}

// Notes(lnikon): Use the same serialized record both in WAL and Raft consensus module
auto db_t::put(
    structures::lsmtree::key_t key, structures::lsmtree::value_t value, db_put_context_k context
) noexcept -> bool
{
    return put(record_t{key, value}, context);
}

[[nodiscard]] auto db_t::put(record_t record, db_put_context_k context) noexcept -> bool
{
    // TODO(lnikon): Should lock a WriteMutex here!

    if (m_pConsensusModule)
    {
        if (context == db_put_context_k::do_not_replicate_k)
        {
            spdlog::debug(
                "db_t::put: Skipping replication for entry: {} {}",
                record.m_key.m_key,
                record.m_value.m_value
            );
        }
        else if (context == db_put_context_k::replicate_k)
        {
            if (m_pConsensusModule->getStateSafe() == NodeState::FOLLOWER)
            {
                spdlog::debug(
                    "db_t::put: Forwarding entry: {} {}", record.m_key.m_key, record.m_value.m_value
                );
                std::stringstream sstream;
                record.write(sstream);
                if (m_pConsensusModule->forward(sstream.str()) !=
                    raft::raft_operation_status_k::success_k)
                {
                    spdlog::error(
                        "db_t::put: Failed to replicate entry: {} {}",
                        record.m_key.m_key,
                        record.m_value.m_value
                    );
                    return false;
                }
            }
        }
        else
        {
            spdlog::debug(
                "db_t::put: Not replicating entry: {} {}",
                record.m_key.m_key,
                record.m_value.m_value
            );
        }
    }

    if (!m_pWal->add({.op = wal::operation_k::add_k, .kv = record}))
    {
        spdlog::error(
            "db_t::put: Failed to put entry: {} {}", record.m_key.m_key, record.m_value.m_value
        );
        return false;
    }

    if (m_pConsensusModule)
    {
        if (context == db_put_context_k::do_not_replicate_k)
        {
            spdlog::debug(
                "db_t::put: Skipping replication for entry: {} {}",
                record.m_key.m_key,
                record.m_value.m_value
            );
        }
        else if (context == db_put_context_k::replicate_k)
        {
            if (m_pConsensusModule->getStateSafe() == NodeState::LEADER)
            {
                spdlog::debug(
                    "db_t::put: Forwarding entry: {} {}", record.m_key.m_key, record.m_value.m_value
                );
                std::stringstream sstream;
                record.write(sstream);
                if (m_pConsensusModule->replicate(sstream.str()) !=
                    raft::raft_operation_status_k::success_k)
                {
                    spdlog::error(
                        "db_t::put: Failed to replicate entry: {} {}",
                        record.m_key.m_key,
                        record.m_value.m_value
                    );
                    return false;
                }
            }
        }
        else
        {
            spdlog::debug(
                "db_t::put: Not replicating entry: {} {}",
                record.m_key.m_key,
                record.m_value.m_value
            );
        }
    }

    switch (m_pLsmtree->put(std::move(record)))
    {
    case structures::lsmtree::lsmtree_status_k::ok_k:
    {
        return true;
    }
    case structures::lsmtree::lsmtree_status_k::memtable_reset_k:
    {
        if (!m_pWal->reset())
        {
            spdlog::error("db_t::put: Failed to reset WAL after memtable reset");
            return false;
        }
        return true;
    }
    default:
    {
        return false;
    }
    }

    return true;
}

auto db_t::get(const structures::lsmtree::key_t &key)
    -> std::optional<structures::memtable::memtable_t::record_t>
{
    // TODO(lnikon): Should lock a ReadMutex here!
    return m_pLsmtree->get(key);
}

auto db_t::config() const noexcept -> config::shared_ptr_t
{
    return m_config;
}

auto db_t::prepare_directory_structure() -> bool
{
    // Create database directory
    if (!std::filesystem::exists(m_config->DatabaseConfig.DatabasePath))
    {
        spdlog::info(
            "Creating database directory at {}", m_config->DatabaseConfig.DatabasePath.c_str()
        );
        if (!std::filesystem::create_directory(m_config->DatabaseConfig.DatabasePath))
        {
            spdlog::error(
                "Failed to create database directory at {}",
                m_config->DatabaseConfig.DatabasePath.c_str()
            );
            return false;
        }
    }
    else
    {
        spdlog::info("Opening database at {}", m_config->DatabaseConfig.DatabasePath.c_str());
    }

    // Create segments directory inside database directory
    const auto &segmentsPath{m_config->datadir_path()};
    if (!std::filesystem::exists(segmentsPath))
    {
        spdlog::info("Creating segments directory at {}", segmentsPath.c_str());
        if (!std::filesystem::create_directory(segmentsPath))
        {
            spdlog::error("Failed to create segments directory at {}", segmentsPath.c_str());
            return false;
        }
    }

    return true;
}

void db_t::swap(db_t &other) noexcept
{
    using std::swap;

    swap(m_config, other.m_config);
    swap(m_pManifest, other.m_pManifest);
    swap(m_pWal, other.m_pWal);
    swap(m_pLsmtree, other.m_pLsmtree);
}

} // namespace db
