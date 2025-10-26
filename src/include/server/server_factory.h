#pragma once

#include <spdlog/spdlog.h>

#include "server/server_kind.h"
#include "server/server_concept.h"
#include "server/grpc_server.h"

namespace server
{

using namespace grpc_communication;

template <communication_strategy_kind_k Type>
    requires communication_strategy_t<grpc_communication_t>
struct CommunicationFactory;

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
