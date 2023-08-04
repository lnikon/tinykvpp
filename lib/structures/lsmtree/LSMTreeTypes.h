//
// Created by nikon on 2/6/22.
//

#ifndef ZKV_LSMTREETYPES_H
#define ZKV_LSMTREETYPES_H

#include <cstddef>
#include <string>
#include <memory>

#include "structures/memtable/MemTable.h"

namespace structures::lsmtree {
    using namespace structures;
    using MemTable = memtable::memtable_t;
    using MemTableUniquePtr = memtable::MemTableUniquePtr;
    using Record = memtable::memtable_t::record_t;
    using Key = MemTable::record_t::key_t;
    using Value = MemTable::record_t::value_t;

    enum class LSMTreeSegmentType
    {
        Mock,
        Regular,

        EnumSize
    };
}

#endif //ZKV_LSMTREETYPES_H
