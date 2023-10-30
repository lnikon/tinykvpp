//
// Created by nikon on 2/6/22.
//

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include <cassert>

#include "lsmtree_regular_segment.h"

namespace structures::lsmtree {

lsmtree_regular_segment_t::lsmtree_regular_segment_t(
    std::string name, memtable_unique_ptr_t pMemtable)
    : interface_lsmtree_segment_t(std::move(name), std::move(pMemtable)) {}

void lsmtree_regular_segment_t::flush() {
  assert(m_pMemtable);
  std::stringstream ss;
  for (const auto &kv : *m_pMemtable) {
    ss << kv;
  }

  // TODO(vahag): Use fadvise() and O_DIRECT
  // TODO(vahag): Async IO?
  std::fstream stream(get_name(), std::fstream::out);
  if (!stream.is_open()) {
    // TODO(vahag): How to handle situation when it's impossible to flush
    // memtable into disk?
    spdlog::error("(lsmtree_regular_segment_t): unable to flush regular "
                  "segment with name=" +
                  get_name() + "\n");
    return;
  }

  stream << ss.str();
  stream.flush();
  stream.close();
}

} // namespace structures::lsmtree
