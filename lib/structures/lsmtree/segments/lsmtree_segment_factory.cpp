//
// Created by nikon on 2/6/22.
//

#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/lsmtree_regular_segment.h>
#include <structures/lsmtree/segments/lsmtree_segment_factory.h>

namespace structures::lsmtree::segments::factories
{

auto lsmtree_segment_factory(types::name_t name, fs::path_t path, memtable::memtable_t memtable)
    -> lsmtree::segments::regular_segment::shared_ptr_t
{
    return regular_segment::make_shared(std::move(path), std::move(name), std::move(memtable));
}

} // namespace structures::lsmtree::segments::factories
