#include "db.h"

namespace db {
/**
 * TODO(lnikon): Introduce DBConfig
 * TODO(lnikon): Move @dbPath and @lsmTreeConfig into DBConfig
 */
db_t::db_t(const config::sptr_t config)
    : m_config{config},
      m_pSegmentManager{segment_manager::make_shared(config)},
      m_lsmTree{config, m_pSegmentManager} {}

bool db_t::open() {
  if (!prepare_directory_structure()) {
		return false;
	}

  return false;
}

bool db_t::prepare_directory_structure() {
  if (!std::filesystem::exists(m_config->DatabaseConfig.DatabasePath)) {
    std::filesystem::create_directory(m_config->DatabaseConfig.DatabasePath);
  }

  return true;
}

} // namespace db
