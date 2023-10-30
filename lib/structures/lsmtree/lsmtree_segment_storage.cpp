#include <cassert>

#include "lsmtree_segment_storage.h"

namespace structures::lsmtree {

segment_shared_ptr_t
lsmtree_segment_storage_t::get(const name_type_t &name) const {
  assert(!name.empty());
  return nullptr;
}

void lsmtree_segment_storage_t::put(segment_shared_ptr_t pLsmTreeSegment) {
  assert(pLsmTreeSegment);
}

void lsmtree_segment_storage_t::remove(const name_type_t &name) {
  assert(!name.empty());
}

} // namespace structures::lsmtree
