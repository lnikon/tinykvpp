//
// Created by nikon on 2/6/22.
//

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include <cassert>

#include <structures/lsmtree/segments/lsmtree_regular_segment.h>

namespace structures::lsmtree {

lsmtree_regular_segment_t::lsmtree_regular_segment_t(
    std::filesystem::path path, memtable_unique_ptr_t pMemtable)
    : interface_lsmtree_segment_t(std::move(path), std::move(pMemtable)) {}

void lsmtree_regular_segment_t::flush() {
  assert(m_pMemtable);
  std::stringstream ss;
  for (const auto &kv : *m_pMemtable) {
    ss << kv;
  }

  // TODO(vahag): Use fadvise() and O_DIRECT
  // TODO(vahag): Async IO?
  std::fstream stream(get_path(), std::fstream::out | std::fstream::app);
  if (!stream.is_open()) {
    // TODO(vahag): How to handle situation when it's impossible to flush
    // memtable into disk?
    spdlog::error("(lsmtree_regular_segment_t): unable to flush regular "
                  "segment with path=" +
                  get_path().string() + "\n");
    return;
  }

  stream << ss.str();
  stream.flush();
  stream.close();
}

} // namespace structures::lsmtree
