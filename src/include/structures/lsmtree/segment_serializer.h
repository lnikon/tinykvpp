#include <cassert>
#include <fstream>
#include <optional>
#include <ios>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>

#include "serialization/buffer_writer.h"
#include "serialization/common.h"
#include "serialization/endian_integer.h"
#include "structures/lsmtree/lsmtree_types.h"
#include "structures/lsmtree/segments/lsmtree_regular_segment.h"
#include "structures/memtable/memtable.h"

namespace structures::lsmtree::segments::regular_segment
{

} // namespace structures::lsmtree::segments::regular_segment
