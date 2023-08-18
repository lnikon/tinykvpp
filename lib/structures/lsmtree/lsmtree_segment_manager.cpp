//
// Created by nikon on 2/6/22.
//

#include "lsmtree_segment_manager.h"
#include "lsmtree_segment_factory.h"
#include "lsmtree_types.h"

namespace structures::lsmtree {
segment_shared_ptr_t lsmtree_segment_manager_t::get_new_segment(
    const structures::lsmtree::lsmtree_segment_type_t type,
    memtable_unique_ptr_t pMemtable) {
  const auto name{get_next_name()};
  auto result = lsmtree_segment_factory(type, name, std::move(pMemtable));
  m_segments[name] = result;
  return result;
}

segment_shared_ptr_t
lsmtree_segment_manager_t::get_segment(const std::string &name) {
  assert(!name.empty());
  segment_shared_ptr_t result{nullptr};
  if (auto it = m_segments.find(name); it != m_segments.end()) {
    result = it->second;
  } else {
    spdlog::warn("unable to find lsm tree segment with name {:s}", name);
  }

  return result;
}

// TODO(vahag): Find better naming strategy
std::string lsmtree_segment_manager_t::get_next_name() {
  return "segment_" + std::to_string(m_index++);
}
} // namespace structures::lsmtree
