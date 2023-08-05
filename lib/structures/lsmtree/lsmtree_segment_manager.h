//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREESEGMENTMANAGER_H
#define ZKV_LSMTREESEGMENTMANAGER_H

#include <unordered_map>

#include "lsmtree_types.h"
#include "interface_lsmtree_segment.h"

namespace structures::lsmtree {
/**
 * Does the management of on-disk segments.
 * On-disk segment is born when memtable is flushed onto the disk.
 */
class lsmtree_segment_manager_t {
public:
  /**
   * TODO: Should be thread-safe?
   */
  shared_ptr_t get_new_segment(const lsmtree_segment_type_t type);
  shared_ptr_t get_segment(const std::string &name);

  // TODO: Start merging on-disk segments.
  // void Compact();
private:
  std::string get_next_name();

private:
  uint64_t m_index{0};
  std::unordered_map<std::string, shared_ptr_t> m_segments;
};

using lsmtree_segment_manager_shared_ptr_t = std::shared_ptr<lsmtree_segment_manager_t>;
} // namespace structures::lsmtree

#endif // ZKV_LSMTREESEGMENTMANAGER_H
