#include "db.h"
#include "db/manifest/manifest.h"

#include <spdlog/spdlog.h>

namespace db
{

db_t::db_t(config::shared_ptr_t config, wal::shared_ptr_t wal)
    : m_config{config},
      m_pManifest{manifest::make_shared(config)},
      m_pWal{std::move(wal)},
      m_lsmTree{config, m_pManifest, m_pWal}
{
}

// TODO(lnikon): 1) Use error_code_t. 2) Move into database_builder_t.
auto db_t::open() -> bool
{
    if (!prepare_directory_structure())
    {
        return false;
    }

    // Open the manifest file
    if (!m_pManifest->open())
    {
        spdlog::error("Unable to open manifest file at {}", m_pManifest->path().c_str());
        return false;
    }

    // Read on-disk components of lsmtree
    if (!m_pManifest->recover())
    {
        // TODO(lnikon): Maybe use error codes?
        spdlog::error("unable to recover manifest file. path={}", m_pManifest->path().string());
        return false;
    }

    // Restore lsmtree based on manifest and WAL
    if (!m_lsmTree.recover())
    {
        spdlog::error("Unable to restore the database at {}",
                      m_config->DatabaseConfig.DatabasePath.c_str());
        return false;
    }

    return true;
}

auto db_t::put(const structures::lsmtree::key_t   &key,
               const structures::lsmtree::value_t &value) noexcept -> bool
{
    return m_lsmTree.put(key, value);
}

auto db_t::get(const structures::lsmtree::key_t &key)
    -> std::optional<structures::memtable::memtable_t::record_t>
{
    return m_lsmTree.get(key);
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
        spdlog::info("Creating database directory at {}",
                     m_config->DatabaseConfig.DatabasePath.c_str());
        if (!std::filesystem::create_directory(m_config->DatabaseConfig.DatabasePath))
        {
            spdlog::error("Failed to create database directory at {}",
                          m_config->DatabaseConfig.DatabasePath.c_str());
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
    swap(m_lsmTree, other.m_lsmTree);
}

} // namespace db
