//
// Created by nikon on 2/6/22.
//

#include "lsmtree_segment_manager.h"
#include "lsmtree_segment_factory.h"
#include "lsmtree_types.h"

namespace structures::lsmtree {
shared_ptr_t lsmtree_segment_manager_t::get_new_segment(
    const structures::lsmtree::lsmtree_segment_type_t type) {
  const auto name{get_next_name()};
  auto result = lsmtree_segment_factory(type, name);
  m_segments[name] = result;
  return result;
}

shared_ptr_t lsmtree_segment_manager_t::get_segment(const std::string &name) {
  assert(!name.empty());
  shared_ptr_t result{nullptr};
  if (auto it = m_segments.find(name); it != m_segments.end()) {
    result = it->second;
  } else {
    spdlog::warn("unable to find lsm tree segment with name {:s}", name);
  }

  return result;
}

std::string lsmtree_segment_manager_t::get_next_name() {
  return "segment_" + std::to_string(m_index++);
}
} // namespace structures::lsmtree
