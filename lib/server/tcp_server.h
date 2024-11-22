#pragma once

#include "server/server_concept.h"
#include "server_kind.h"

namespace server::tcp_communication
{

class tcp_communication_t final
{
  public:
    static constexpr const auto kind = communication_strategy_kind_k::tcp_k;

    void start(db::db_t &db) const noexcept;
    void shutdown() const noexcept;
};

static_assert(server::communication_strategy_t<tcp_communication_t>,
              "TCPServerCommunication must satisfy CommunicationStrategy concept");

} // namespace server::tcp_communication
