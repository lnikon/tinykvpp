#pragma once

#include <structures/lsmtree/lsmtree_config.h>

#include "fs/types.h"

namespace db
{

struct db_config_t
{
    fs::path_t  DatabasePath{"."};
    std::string WalFilename{"wal"};
    std::string ManifestFilenamePrefix{"manifest_"};
};

} // namespace db
