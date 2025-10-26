#include "wal/wal.h"

namespace wal
{

auto wal_builder_t::set_file_path(fs::path_t path) -> wal_builder_t &
{
    m_path = std::move(path);
    return *this;
}

} // namespace wal
