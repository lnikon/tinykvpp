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
    using MemTable = memtable::MemTable;
    using MemTableUniquePtr = memtable::MemTableUniquePtr;
    using Record = memtable::MemTable::Record;
    using Key = MemTable::Record::Key;
    using Value = MemTable::Record::Value;

    enum class LSMTreeSegmentType
    {
        Mock,
        Regular,

        EnumSize
    };
}

#endif //ZKV_LSMTREETYPES_H
