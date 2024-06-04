//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREESEGMENTFACTORY_H
#define ZKV_LSMTREESEGMENTFACTORY_H

#include <structures/lsmtree/lsmtree_types.h>
#include <structures/lsmtree/segments/segment_interface.h>

#include <filesystem>

namespace structures::lsmtree::segments::factories
{

// TODO: Variadic template?
interface::shared_ptr_t lsmtree_segment_factory(
    const lsmtree_segment_type_t type,
    types::name_t name,
    types::path_t path,
    memtable::memtable_t memtable);
}  // namespace structures::lsmtree::segments::factories

#endif  // ZKV_LSMTREESEGMENTFACTORY_H
