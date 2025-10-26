#pragma once

#include "lsmtree_regular_segment.h"
#include "types.h"

namespace structures::lsmtree::segments::factories
{

auto lsmtree_segment_factory(types::name_t name, fs::path_t path, memtable::memtable_t memtable)
    -> lsmtree::segments::regular_segment::shared_ptr_t;

} // namespace structures::lsmtree::segments::factories
