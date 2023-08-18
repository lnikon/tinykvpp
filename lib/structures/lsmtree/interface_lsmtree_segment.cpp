//
// Created by nikon on 2/6/22.
//

#define FMT_HEADER_ONLY
#include <spdlog/spdlog.h>

#include "interface_lsmtree_segment.h"

namespace structures::lsmtree {

interface_lsmtree_segment_t::interface_lsmtree_segment_t(
    std::string name, memtable_unique_ptr_t pMemtable)
    : m_pMemtable(std::move(pMemtable)), m_name(std::move(name)) {}

std::string interface_lsmtree_segment_t::get_name() const { return m_name; }

} // namespace structures::lsmtree
