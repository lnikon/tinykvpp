#pragma once

#include "db/db.h"
#include "server_kind.h"

#include <fmt/core.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

#include "TinyKVPP.grpc.pb.h"
#include "TinyKVPP.pb.h"

class TinyKVPPServiceImpl final : public TinyKVPPService::Service
{
  public:
    TinyKVPPServiceImpl(db::db_t &db)
        : m_db(db)
    {
    }

    auto Put(grpc::ServerContext *pContext, const PutRequest *pRequest, PutResponse *pResponse) -> grpc::Status override
    {
        (void)pContext;

        m_db.put(structures::lsmtree::key_t{pRequest->key()}, structures::lsmtree::value_t{pRequest->value()});
        pResponse->set_status(std::string("OK"));
        return grpc::Status::OK;
    }

    auto Get(grpc::ServerContext *pContext, const GetRequest *pRequest, GetResponse *pResponse) -> grpc::Status override
    {
        (void)pContext;

        const auto &record = m_db.get(structures::lsmtree::key_t{pRequest->key()});
        if (record)
        {
            pResponse->set_value(record.value().m_value.m_value);
        }
        return grpc::Status::OK;
    }

  private:
    db::db_t &m_db;
};

class GRPCCommunication
{
  public:
    static constexpr const auto kind = CommunicationStrategyKind::GRPC;

    void start(db::db_t &db) const
    {
        spdlog::info("Starting gRPC communication...");
        TinyKVPPServiceImpl service(db);
        grpc::ServerBuilder builder;
        const auto          dbConfig{db.config()};
        const auto serverAddress{fmt::format("{}:{}", dbConfig->ServerConfig.host, dbConfig->ServerConfig.port)};
        builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        spdlog::info("Server listening on {}", serverAddress);
        server->Wait();
    }
};
