#include "db.h"

namespace db
{
/**
 * TODO(lnikon): Introduce DBConfig
 * TODO(lnikon): Move @dbPath and @lsmTreeConfig into DBConfig
 */
db_t::db_t(const config::shared_ptr_t config)
    : m_config{config},
      m_pSegmentStorage{lsmtree::segments::storage::make_shared()},
      m_lsmTree{config}
{
}

bool db_t::open()
{
    if (!prepare_directory_structure())
    {
        return false;
    }

    // TODO(lnikon): DB recovery happens here
    // TODO(lnikon): Init thread pool here?
    // TODO(lnikon): Allocate separate thread for compactation inside the thread
    // pool

    return true;
}

// TODO(lnikon): Indicate on insertion failure
void db_t::put(const structures::lsmtree::key_t &key,
               const structures::lsmtree::value_t &value)
{
    m_lsmTree.put(key, value);
}

std::optional<record_t> db_t::get(const structures::lsmtree::key_t &key) const
{
    return m_lsmTree.get(key);
}

bool db_t::prepare_directory_structure()
{
    if (!std::filesystem::exists(m_config->DatabaseConfig.DatabasePath))
    {
        std::filesystem::create_directory(
            m_config->DatabaseConfig.DatabasePath);
    }

    const auto &segmentsPath{m_config->datadir_path()};
    if (!std::filesystem::exists(segmentsPath))
    {
        std::filesystem::create_directory(segmentsPath);
    }

    return true;
}

}  // namespace db
