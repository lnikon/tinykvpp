#ifndef LSM_TREE_SEGMENT_STORAGE
#define LSM_TREE_SEGMENT_STORAGE

#include <string>
#include <unordered_map>

#include "interface_lsmtree_segment.h"

namespace structures::lsmtree {

class lsmtree_segment_storage_t {
public:
  using name_type_t = std::string;

  segment_shared_ptr_t get(const name_type_t &name) const;
  void put(segment_shared_ptr_t pLsmTreeSegment);
  void remove(const name_type_t &name);

private:
  std::unordered_map<name_type_t, segment_shared_ptr_t> m_segments;
};

} // namespace structures::lsmtree

#endif // LSM_TREE_SEGMENT_STORAGE
