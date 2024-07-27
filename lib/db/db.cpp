#include "db.h"

#include <spdlog/spdlog.h>

namespace db
{
/**
 * TODO(lnikon): Introduce DBConfig
 * TODO(lnikon): Move @dbPath and @lsmTreeConfig into DBConfig
 */
db_t::db_t(const config::shared_ptr_t config)
    : m_config{config},
      m_manifest{manifest::make_shared(config->DatabaseConfig.DatabasePath / "manifest")}, // TODO: use helper function
      m_wal{wal::make_shared(config->DatabaseConfig.DatabasePath / "wal")},                // TODO: use helper function
      m_lsmTree{config, m_manifest, m_wal}
{
}

bool db_t::open()
{
    if (!prepare_directory_structure())
    {
        return false;
    }

    // Read on-disk components of lsmtree
    if (!m_manifest->recover())
    {
        // TODO(lnikon): Maybe use error codes?
        spdlog::error("unable to recover manifest file. path={}", m_manifest->path().string());
        return false;
    }

    // Restore lsmtree based on manifest and WAL
    m_lsmTree.restore();

    return true;
}

// TODO(lnikon): Indicate on insertion failure
void db_t::put(const structures::lsmtree::key_t &key, const structures::lsmtree::value_t &value)
{
    m_lsmTree.put(key, value);
}

std::optional<record_t> db_t::get(const structures::lsmtree::key_t &key)
{
    return m_lsmTree.get(key);
}

bool db_t::prepare_directory_structure()
{
    if (!std::filesystem::exists(m_config->DatabaseConfig.DatabasePath))
    {
        std::filesystem::create_directory(m_config->DatabaseConfig.DatabasePath);
    }

    const auto &segmentsPath{m_config->datadir_path()};
    if (!std::filesystem::exists(segmentsPath))
    {
        std::filesystem::create_directory(segmentsPath);
    }

    return true;
}

} // namespace db
