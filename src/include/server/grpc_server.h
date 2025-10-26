#pragma once

#include <grpcpp/grpcpp.h>

#include "server/server_concept.h"
#include "db/db.h"
#include "server/server_kind.h"

#include "tinykvpp/v1/tinykvpp_service.grpc.pb.h"
#include "tinykvpp/v1/tinykvpp_service.pb.h"

namespace server::grpc_communication
{

class tinykvpp_service_impl_t final : public tinykvpp::v1::TinyKVPPService::Service
{
  public:
    explicit tinykvpp_service_impl_t(db::shared_ptr_t db);

    auto
    Put(grpc::ServerContext            *pContext,
        const tinykvpp::v1::PutRequest *pRequest,
        tinykvpp::v1::PutResponse      *pResponse) -> grpc::Status override;

    auto
    Get(grpc::ServerContext            *pContext,
        const tinykvpp::v1::GetRequest *pRequest,
        tinykvpp::v1::GetResponse      *pResponse) -> grpc::Status override;

  private:
    db::shared_ptr_t m_database;
};

class grpc_communication_t final
{
  public:
    static constexpr const auto kind = communication_strategy_kind_k::grpc_k;

    void start(db::shared_ptr_t database) noexcept;
    void shutdown() noexcept;

  private:
    std::unique_ptr<grpc::Server>            m_server{nullptr};
    std::unique_ptr<tinykvpp_service_impl_t> m_service{nullptr};
};

static_assert(
    communication_strategy_t<grpc_communication_t>,
    "GRPCCommunication must satisfy CommunicationStrategy concept"
);

} // namespace server::grpc_communication
