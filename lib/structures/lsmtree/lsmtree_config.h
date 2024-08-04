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
    /*
     * Determines the size (in Mb) of the in-memory memtable after which it should be flushed
     * onto the disk.
     */
    const uint64_t DefaultDiskFlushThresholdSize{8 * 1024 * 1024};
    uint64_t DiskFlushThresholdSize{DefaultDiskFlushThresholdSize};

    /*
     * Determines number of segments after whicqh compaction process should
     * start.
     */
    const uint64_t DefaultLevelZeroCompactionSegmentCount{4};
    uint64_t LevelZeroCompactionSegmentCount{DefaultLevelZeroCompactionSegmentCount};

    /*
     * Type of the segment that LSMTree should use.
     */
    const lsmtree_segment_type_t DefaultSegmentType{lsmtree_segment_type_t::regular_k};
    lsmtree_segment_type_t SegmentType{DefaultSegmentType};

    /*
     * Name of directory inside the database root dir where segments should be
     * stored
     */
    const std::string DefaultSegmentsDirectoryName = "segments";
    std::string SegmentsDirectoryName{DefaultSegmentsDirectoryName};
};

} // namespace structures::lsmtree

#endif // ZKV_LSMTREECONFIG_H
