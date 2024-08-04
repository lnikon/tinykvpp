#include "db.h"
#include "db/manifest/manifest.h"

#include <spdlog/spdlog.h>

namespace db
{

db_t::db_t(config::shared_ptr_t config)
    : m_config{config},
      m_manifest{manifest::make_shared(config)},
      m_wal{wal::make_shared(config->DatabaseConfig.DatabasePath / wal::wal_filename())},
      m_lsmTree{config, m_manifest, m_wal}
{
}

// TODO(lnikon): use error_code_t
auto db_t::open() -> bool
{
    if (!prepare_directory_structure())
    {
        return false;
    }

    // Open the manifest file
    if (!m_manifest->open())
    {
        spdlog::error("Unable to open manifest file at {}", m_manifest->path().c_str());
        return false;
    }

    // Read on-disk components of lsmtree
    if (!m_manifest->recover())
    {
        // TODO(lnikon): Maybe use error codes?
        spdlog::error("unable to recover manifest file. path={}", m_manifest->path().string());
        return false;
    }

    // Open the WAL
    if (!m_wal->open())
    {
        spdlog::error("unable to open WAL file. path={}", m_wal->path().string());
        return false;
    }

    // Recover WAL
    if (!m_wal->recover())
    {
        spdlog::error("unable to recover WAL file. path={}", m_wal->path().string());
        return false;
    }

    // Restore lsmtree based on manifest and WAL
    if (!m_lsmTree.recover())
    {
        spdlog::error("Unable to restore the database at {}", m_config->DatabaseConfig.DatabasePath.c_str());
        return false;
    }

    return true;
}

// TODO(lnikon): Indicate on insertion failure
void db_t::put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value)
{
    m_lsmTree.put(key, value);
}

auto db_t::get(const structures::lsmtree::key_t &key) -> std::optional<structures::memtable::memtable_t::record_t>
{
    return m_lsmTree.get(key);
}

auto db_t::prepare_directory_structure() -> bool
{
    // Create database directory
    if (!std::filesystem::exists(m_config->DatabaseConfig.DatabasePath))
    {
        spdlog::info("Creating database directory at {}", m_config->DatabaseConfig.DatabasePath.c_str());
        if (!std::filesystem::create_directory(m_config->DatabaseConfig.DatabasePath))
        {
            spdlog::error("Failed to create database directory at {}", m_config->DatabaseConfig.DatabasePath.c_str());
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

} // namespace db
