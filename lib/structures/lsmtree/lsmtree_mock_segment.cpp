//
// Created by nikon on 2/6/22.
//

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include <cassert>

#include "lsmtree_mock_segment.h"

namespace structures::lsmtree {

lsmtree_mock_segment_t::lsmtree_mock_segment_t(std::string name,
                                               memtable_unique_ptr_t pMemtable)
    : interface_lsmtree_segment_t(std::move(name), std::move(pMemtable)) {}

void lsmtree_mock_segment_t::flush() {
  assert(m_pMemtable);
}

} // namespace structures::lsmtree
