#pragma once

#include <chrono>
#include <libassert/assert.hpp>

#include "fs/types.h"

namespace db
{

struct db_config_t
{
    fs::path_t  DatabasePath{"."};
    std::string WalFilename{"wal"};
    std::string ManifestFilenamePrefix{"manifest_"};

    std::size_t               requestQueueSize{100'000};
    std::size_t               threadPoolSize{16};
    std::size_t               maxPendingRequests = 10'000;
    std::chrono::milliseconds requestTimeout{5000};
    std::chrono::milliseconds forwardTimeout{1000};
    std::size_t               maxForwardRetries{3};
};

} // namespace db
