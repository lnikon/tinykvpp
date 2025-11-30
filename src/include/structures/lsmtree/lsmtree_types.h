#pragma once

#include "structures/memtable/memtable.h"

namespace structures::lsmtree
{

using namespace structures;

using memtable_t = memtable::memtable_t;
using record_t = memtable::memtable_t::record_t;
using key_t = memtable_t::record_t::key_t;
using value_t = memtable_t::record_t::value_t;
using sequence_number_t = memtable_t::record_t::sequence_number_t;
using timestamp_t = memtable_t::record_t::timestamp_t;

} // namespace structures::lsmtree
