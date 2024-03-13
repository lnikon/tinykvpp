//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREETYPES_H
#define ZKV_LSMTREETYPES_H

#include <structures/memtable/memtable.h>

namespace structures::lsmtree
{

using namespace structures;

using memtable_t = memtable::memtable_t;
using memtable_unique_ptr_t = memtable::unique_ptr_t;
using record_t = memtable::memtable_t::record_t;
using key_t = memtable_t::record_t::key_t;
using value_t = memtable_t::record_t::value_t;

enum class lsmtree_segment_type_t
{
    mock_k = 0,
    regular_k,
};

}  // namespace structures::lsmtree

#endif  // ZKV_LSMTREETYPES_H
