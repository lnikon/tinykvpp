//
// Created by nikon on 2/6/22.
//

#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/lsmtree_mock_segment.h>
#include <structures/lsmtree/segments/lsmtree_regular_segment.h>
#include <structures/lsmtree/segments/lsmtree_segment_factory.h>

namespace structures::lsmtree {

segment_shared_ptr_t lsmtree_segment_factory(const lsmtree_segment_type_t type,
                                             std::filesystem::path path,
                                             memtable_unique_ptr_t pMemtable) {
  switch (type) {
  case lsmtree_segment_type_t::mock_k:
    return std::make_shared<lsmtree_mock_segment_t>(std::move(path),
                                                    std::move(pMemtable));
  case lsmtree_segment_type_t::regular_k:
    return std::make_shared<lsmtree_regular_segment_t>(std::move(path),
                                                       std::move(pMemtable));
  default:
    spdlog::error("unhandled lsm tree segment type");
    assert(false);
    return nullptr;
  }
}

} // namespace structures::lsmtree
