#pragma once

#include <cstdint>
#include <string>

namespace structures::lsmtree
{

struct lsmtree_config_t
{
    /**
     * Determines the size (in Mb) of the in-memory memtable after which it
     * should be flushed onto the disk.
     */
    std::uint64_t DiskFlushThresholdSize{8 * 1024 * 1024};

    /**
     * Determines number of segments after which compaction process should
     * start for level 0.
     */
    std::uint64_t LevelZeroCompactionThreshold{0};

    /**
     * Determines strategy used by compaction process  for level 0.
     * TODO(lnikon): Switch to enum
     */
    std::string LevelZeroCompactionStrategy;

    /**
     * Determines number of segments after which compaction process should
     * start for level 1 and below.
     */
    std::uint64_t LevelNonZeroCompactionThreshold{0};

    /**
     * Determines strategy used by compaction process for level 1 and below.
     * TODO(lnikon): Switch to enum
     */
    std::string LevelNonZeroCompactionStrategy;
};

} // namespace structures::lsmtree
