//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREECONFIG_H
#define ZKV_LSMTREECONFIG_H

namespace structures::lsmtree {
    struct LSMTreeConfig {
        /*
         * Determines the size (in Mb) of the table after which it should be flushed onto the disk.
         */
        const uint64_t DefaultDiskFlushThresholdSize{8 * 1024 * 1024}; // 8 Megabyte
        uint64_t DiskFlushThresholdSize{DefaultDiskFlushThresholdSize};

        /*
         * Determines number of segments after whicqh compaction process should start.
         */
        const uint64_t DefaultCompactionSegmentCount{16};
        uint64_t CompactionSegmentCount{DefaultCompactionSegmentCount};

        /*
         * Type of the segment that LSMTree should use.
         */
        const LSMTreeSegmentType DefaultSegmentType{LSMTreeSegmentType::Regular};
        LSMTreeSegmentType SegmentType{DefaultSegmentType};
    };
}

#endif //ZKV_LSMTREECONFIG_H
