#include "server/grpc_server.h"

#include <fmt/core.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

namespace server::grpc_communication
{

tinykvpp_service_impl_t::tinykvpp_service_impl_t(db::db_t &db)
    : m_db(db)
{
}

auto tinykvpp_service_impl_t::Put(grpc::ServerContext *pContext,
                                  const PutRequest    *pRequest,
                                  PutResponse         *pResponse) -> grpc::Status
{
    (void)pContext;

    m_db.put(structures::lsmtree::key_t{pRequest->key()}, structures::lsmtree::value_t{pRequest->value()});
    pResponse->set_status(std::string("OK"));
    return grpc::Status::OK;
}

auto tinykvpp_service_impl_t::Get(grpc::ServerContext *pContext,
                                  const GetRequest    *pRequest,
                                  GetResponse         *pResponse) -> grpc::Status
{
    (void)pContext;

    const auto &record = m_db.get(structures::lsmtree::key_t{pRequest->key()});
    if (record)
    {
        pResponse->set_value(record.value().m_value.m_value);
    }
    return grpc::Status::OK;
}
void grpc_communication_t::start(db::db_t &db) const noexcept
{
    spdlog::info("Starting gRPC communication...");
    tinykvpp_service_impl_t service(db);
    grpc::ServerBuilder     builder;
    const auto              dbConfig{db.config()};
    const auto serverAddress{fmt::format("{}:{}", dbConfig->ServerConfig.host, dbConfig->ServerConfig.port)};
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("Server listening on {}", serverAddress);
    server->Wait();
}

} // namespace server::grpc_communication
