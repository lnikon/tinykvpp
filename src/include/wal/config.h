#pragma once

#include "fs/types.h"
#include "wal/common.h"

namespace wal
{

struct config_t
{
    bool               enable{false};
    fs::path_t         path;
    log_storage_type_k storageType{log_storage_type_k::in_memory_k};
};

} // namespace wal
