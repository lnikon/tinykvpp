#include <db/wal/wal.h>

#include <string>

namespace db::wal
{

std::string wal_filename()
{
    return std::string("wal");
}

} // namespace db::wal