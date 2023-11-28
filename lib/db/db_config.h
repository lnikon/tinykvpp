#pragma once

#include <structures/lsmtree/lsmtree_config.h>

#include <filesystem>

namespace db {

struct db_config_t {
  std::filesystem::path dbPath;
  structures::lsmtree::lsmtree_config_t lsmTreeConfig;
};

} // namespace db
