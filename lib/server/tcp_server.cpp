#include "server/tcp_server.h"

namespace server::tcp_communication
{

void tcp_communication_t::start(db::db_t &db) const noexcept
{
    (void)db;
    spdlog::error("TCP server is not implemented yet");
}

void tcp_communication_t::shutdown() const noexcept
{
}

} // namespace server::tcp_communication
