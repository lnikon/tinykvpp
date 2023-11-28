#include "db.h"

namespace db {
/**
 * TODO(lnikon): Introduce DBConfig
 * TODO(lnikon): Move @dbPath and @lsmTreeConfig into DBConfig
 */
db_t::db_t(const db_config_t &config)
    : m_config{config}, m_pSegmentManager{segment_manager::make_shared(m_config.dbPath)},
      m_lsmTree{lsmtree_config_t{}, m_pSegmentManager} {}

bool db_t::open() { return false; }

bool db_t::prepare_directory_structure() { return false; }

} // namespace db
