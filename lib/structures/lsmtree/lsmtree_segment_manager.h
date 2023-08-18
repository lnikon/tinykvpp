//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREESEGMENTMANAGER_H
#define ZKV_LSMTREESEGMENTMANAGER_H

#include <unordered_map>

#include "interface_lsmtree_segment.h"
#include "lsmtree_types.h"

namespace structures::lsmtree {
/**
 * Does the management of on-disk segments.
 * On-disk segment is born when memtable is flushed onto the disk.
 */
class lsmtree_segment_manager_t {
public:
  using segment_map_t = std::unordered_map<std::string, segment_shared_ptr_t>;
  /**
   * TODO: Should be thread-safe?
   */
  segment_shared_ptr_t get_new_segment(const lsmtree_segment_type_t type, memtable_unique_ptr_t pMemtable);
  segment_shared_ptr_t get_segment(const std::string &name);

  // TODO: Start merging on-disk segments.
  // void Compact();
private:
  std::string get_next_name();

private:
  uint64_t m_index{0};
  segment_map_t m_segments;
};

using lsmtree_segment_manager_shared_ptr_t =
    std::shared_ptr<lsmtree_segment_manager_t>;
} // namespace structures::lsmtree

#endif // ZKV_LSMTREESEGMENTMANAGER_H
