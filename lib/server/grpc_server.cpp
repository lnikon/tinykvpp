#include "server/grpc_server.h"

#include <cstdlib>
#include <fmt/core.h>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

namespace server::grpc_communication
{

tinykvpp_service_impl_t::tinykvpp_service_impl_t(db::db_t &db)
    : m_db(db)
{
}

auto tinykvpp_service_impl_t::Put(grpc::ServerContext *pContext, const PutRequest *pRequest, PutResponse *pResponse)
    -> grpc::Status
{
    (void)pContext;

    m_db.put(structures::lsmtree::key_t{pRequest->key()}, structures::lsmtree::value_t{pRequest->value()});
    pResponse->set_status(std::string("OK"));
    return grpc::Status::OK;
}

auto tinykvpp_service_impl_t::Get(grpc::ServerContext *pContext, const GetRequest *pRequest, GetResponse *pResponse)
    -> grpc::Status
{
    (void)pContext;

    const auto &record = m_db.get(structures::lsmtree::key_t{pRequest->key()});
    if (record)
    {
        pResponse->set_value(record.value().m_value.m_value);
    }
    return grpc::Status::OK;
}

void grpc_communication_t::start(db::db_t &db) noexcept
{
    if (m_server)
    {
        spdlog::warn("gRPC already started");
        return;
    }

    spdlog::info("Starting gRPC communication...");

    const auto serverAddress{fmt::format("{}:{}", db.config()->ServerConfig.host, db.config()->ServerConfig.port)};

    try
    {
        grpc::ServerBuilder builder;
        builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());

        tinykvpp_service_impl_t service(db);
        builder.RegisterService(&service);

        m_server = std::unique_ptr<grpc::Server>(builder.BuildAndStart());
        spdlog::info("Server listening on {}", serverAddress);
        m_server->Wait();
    }
    catch (std::exception &e)
    {
        spdlog::error("Excetion occured while creating gRPC server. {}", e.what());
        exit(EXIT_FAILURE);
    }
}

void grpc_communication_t::shutdown() noexcept
{
    try
    {
        m_server->Shutdown();
    }
    catch (std::exception &e)
    {

        spdlog::error("Excetion occured while shutting down gRPC server. {}", e.what());
        exit(EXIT_FAILURE);
    }
}

} // namespace server::grpc_communication
