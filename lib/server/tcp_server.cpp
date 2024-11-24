#include "server/tcp_server.h"

namespace server::tcp_communication
{

void tcp_communication_t::start(db::shared_ptr_t database) const noexcept
{
    (void)database;
    spdlog::error("TCP server is not implemented yet");
}

void tcp_communication_t::shutdown() const noexcept
{
}

} // namespace server::tcp_communication
