//
// Created by nikon on 2/6/22.
//

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include <cassert>

#include "lsmtree_mock_segment.h"

namespace structures::lsmtree {

lsmtree_mock_segment_t::lsmtree_mock_segment_t(std::string name)
    : interface_lsmtree_segment_t(std::move(name)) {}

void lsmtree_mock_segment_t::flush() {
  assert(!m_content.empty());
  spdlog::info("mock segment flush");
  spdlog::info("going to flash {:s}", m_content);
}

} // namespace structures::lsmtree
