#pragma once

#include "server_kind.h"
#include "server_concept.h"
#include "tcp_server.h"
#include "grpc_server.h"
#include <spdlog/spdlog.h>

namespace server
{

using namespace tcp_communication;
using namespace grpc_communication;

template <communication_strategy_kind_k Type>
    requires communication_strategy_t<tcp_communication_t> ||
             communication_strategy_t<grpc_communication_t>
struct CommunicationFactory;

template <> struct CommunicationFactory<tcp_communication_t::kind>
{
    using type = tcp_communication_t;
};

template <> struct CommunicationFactory<grpc_communication_t::kind>
{
    using type = grpc_communication_t;
};

template <communication_strategy_kind_k kind> [[nodiscard]] auto factory() noexcept
{
    try
    {
        return typename CommunicationFactory<kind>::type();
    }
    catch (std::exception &e)
    {
        spdlog::error("Exception happened during communication creation. {}", e.what());
        throw;
    }
}

} // namespace server
