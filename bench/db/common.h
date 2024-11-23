#include "structures/memtable/memtable.h"

namespace bench
{

using mem_key_t = structures::memtable::memtable_t::record_t::key_t;
using mem_value_t = structures::memtable::memtable_t::record_t::value_t;

auto generateRandomString(const std::size_t length) -> std::string;

} // namespace bench
