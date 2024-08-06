//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREECONFIG_H
#define ZKV_LSMTREECONFIG_H

#include <structures/lsmtree/lsmtree_types.h>

#include <cstdint>

namespace structures::lsmtree
{

struct lsmtree_config_t
{
    /**
     * Determines the size (in Mb) of the in-memory memtable after which it should be flushed
     * onto the disk.
     */
    uint64_t DiskFlushThresholdSize{8 * 1024 * 1024};

    /**
     * Determines number of segments after whicqh compaction process should
     * start.
     */
    uint64_t LevelZeroCompactionThreshold{0};

    /**
     * Determines strategy used by compaction process
     * TODO(lnikon): Switch to enum
     */
    std::string LevelZeroCompactionStrategy;

    /**
     * Determines number of segments after whicqh compaction process should
     * start.
     */
    uint64_t LevelNonZeroCompactionThreshold{0};

    /**
     * Determines strategy used by compaction process
     * TODO(lnikon): Switch to enum
     */
    std::string LevelNonZeroCompactionStrategy;
};

} // namespace structures::lsmtree

#endif // ZKV_LSMTREECONFIG_H
