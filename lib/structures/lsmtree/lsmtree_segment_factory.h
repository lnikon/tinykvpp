//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREESEGMENTFACTORY_H
#define ZKV_LSMTREESEGMENTFACTORY_H

#include "interface_lsmtree_segment.h"
#include "lsmtree_types.h"

namespace structures::lsmtree {

segment_shared_ptr_t lsmtree_segment_factory(const lsmtree_segment_type_t type,
                                             std::string name,
                                             memtable_unique_ptr_t pMemtable);
}

#endif // ZKV_LSMTREESEGMENTFACTORY_H
