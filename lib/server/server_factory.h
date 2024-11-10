#pragma once

#include "server_kind.h"
#include "server_concept.h"
#include "tcp_server.h"
#include "grpc_server.h"

template <CommunicationStrategyKind Type>
    requires CommunicationStrategy<TCPServerCommunication> || CommunicationStrategy<GRPCCommunication>
struct CommunicationFactory;

template <> struct CommunicationFactory<TCPServerCommunication::kind>
{
    using type = TCPServerCommunication;
};

template <> struct CommunicationFactory<GRPCCommunication::kind>
{
    using type = GRPCCommunication;
};

template <CommunicationStrategyKind kind> auto factory()
{
    return typename CommunicationFactory<kind>::type();
}
