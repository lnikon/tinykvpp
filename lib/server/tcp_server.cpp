#include "server/tcp_server.h"

namespace server::tcp_communication
{

void tcp_communication_t::start(db::db_t &db) const noexcept
{
    (void)db;
    std::cout << "Starting TCP Server communication..." << std::endl;
}

} // namespace server::tcp_communication
