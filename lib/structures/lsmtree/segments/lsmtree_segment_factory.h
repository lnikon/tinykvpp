//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREESEGMENTFACTORY_H
#define ZKV_LSMTREESEGMENTFACTORY_H

#include "structures/lsmtree/segments/lsmtree_regular_segment.h"
#include <structures/lsmtree/lsmtree_types.h>

namespace structures::lsmtree::segments::factories
{

lsmtree::segments::regular_segment::shared_ptr_t
lsmtree_segment_factory(types::name_t name, types::path_t path, memtable::memtable_t memtable);

} // namespace structures::lsmtree::segments::factories

#endif // ZKV_LSMTREESEGMENTFACTORY_H
