#include "server/grpc_server.h"
#include "db.h"

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

tinykvpp_service_impl_t::tinykvpp_service_impl_t(db::shared_ptr_t db)
    : m_database(std::move(db))
{
}

auto tinykvpp_service_impl_t::Put(
    grpc::ServerContext *pContext, const PutRequest *pRequest, PutResponse *pResponse
) -> grpc::Status
{
    (void)pContext;

    try
    {
        const bool success = m_database->put(
            db::db_t::key_t{pRequest->key()},
            db::db_t::value_t{pRequest->value()},
            db::db_put_context_k::replicate_k
        );

        if (!success)
        {
            spdlog::warn("Put operation failed for key: {}", pRequest->key());
            pResponse->set_status("FAILED");
        }
        else
        {
            pResponse->set_status("OK");
        }
        return grpc::Status::OK;
    }
    catch (std::exception &e)
    {
        auto msg{fmt::format("Failed to put key-value pair: {}", e.what())};
        spdlog::error(msg);
        return {grpc::StatusCode::INTERNAL, msg};
    }
}

auto tinykvpp_service_impl_t::Get(
    grpc::ServerContext *pContext, const GetRequest *pRequest, GetResponse *pResponse
) -> grpc::Status
{
    (void)pContext;
    try
    {
        const auto &record = m_database->get(db::db_t::key_t{pRequest->key()});
        if (record)
        {
            pResponse->set_value(record.value().m_value.m_value);
        }
        return grpc::Status::OK;
    }
    catch (std::exception &e)
    {
        auto msg{fmt::format("Failed to get key-value pair: {}", e.what())};
        spdlog::error(msg);
        return {grpc::StatusCode::INTERNAL, msg};
    }
}

void grpc_communication_t::start(db::shared_ptr_t database) noexcept
{
    if (m_service && m_server)
    {
        spdlog::warn("gRPC already started");
        return;
    }

    if (!m_service && m_server)
    {
        spdlog::error("gRPC service is not initialized");
        exit(EXIT_FAILURE);
    }

    if (!m_server && m_service)
    {
        spdlog::error("gRPC server is not initialized");
        exit(EXIT_FAILURE);
    }

    if (!database)
    {
        spdlog::error("Database is not initialized");
        exit(EXIT_FAILURE);
    }

    spdlog::info("Starting gRPC communication...");

    const auto serverAddress{fmt::format(
        "{}:{}", database->config()->ServerConfig.host, database->config()->ServerConfig.port
    )};

    try
    {
        grpc::ServerBuilder builder;
        builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());

        m_service = std::make_unique<tinykvpp_service_impl_t>(database);
        builder.RegisterService(m_service.get());

        m_server = std::unique_ptr<grpc::Server>(builder.BuildAndStart());
        if (!m_server)
        {
            spdlog::error("Failed to create gRPC server on {}", serverAddress);
            exit(EXIT_FAILURE);
        }

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
