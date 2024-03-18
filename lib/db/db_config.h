#pragma once

#include <structures/lsmtree/lsmtree_config.h>

#include <filesystem>

namespace db
{

struct db_config_t
{
    const std::filesystem::path DefaultDabasePath{"."};
    std::filesystem::path DatabasePath{DefaultDabasePath};
};

}  // namespace db
