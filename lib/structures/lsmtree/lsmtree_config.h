//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREECONFIG_H
#define ZKV_LSMTREECONFIG_H

#include <cstdint>

#include <structures/lsmtree/lsmtree_types.h>

namespace structures::lsmtree {

struct lsmtree_config_t {
  /*
   * Determines the size (in Mb) of the table after which it should be flushed
   * onto the disk.
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
  const lsmtree_segment_type_t DefaultSegmentType{
      lsmtree_segment_type_t::regular_k};
  lsmtree_segment_type_t SegmentType{DefaultSegmentType};
};

} // namespace structures::lsmtree

#endif // ZKV_LSMTREECONFIG_H
