//
// Created by nikon on 2/6/22.
//

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include <structures/lsmtree/segments/interface_lsmtree_segment.h>

namespace structures::lsmtree {

interface_lsmtree_segment_t::interface_lsmtree_segment_t(
    std::filesystem::path path, memtable_unique_ptr_t pMemtable)
    : m_pMemtable(std::move(pMemtable)), m_path(std::move(path)) {}

std::string interface_lsmtree_segment_t::get_name() const {
  return m_path.stem().string();
}

std::filesystem::path interface_lsmtree_segment_t::get_path() const { return m_path; }

} // namespace structures::lsmtree
