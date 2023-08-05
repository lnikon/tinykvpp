//
// Created by nikon on 2/6/22.
//

#include "lsmtree_segment_factory.h"
#include "lsmtree_mock_segment.h"
#include "lsmtree_regular_segment.h"
#include "structures/lsmtree/lsmtree_types.h"

namespace structures::lsmtree {

shared_ptr_t lsmtree_segment_factory(const lsmtree_segment_type_t type,
                                     std::string name) {
  switch (type) {
  case lsmtree_segment_type_t::mock_k:
    return std::make_shared<lsmtree_mock_segment_t>(name);
  case lsmtree_segment_type_t::regular_k:
    return std::make_shared<lsmtree_regular_segment_t>(name);
  default:
    spdlog::error("unhandled lsm tree segment type");
    assert(false);
    return nullptr;
  }
}

} // namespace structures::lsmtree
