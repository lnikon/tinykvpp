#pragma once

#include "db/db.h"
#include "server/server_concept.h"
#include "server_kind.h"

#include "TinyKVPP.grpc.pb.h"
#include "TinyKVPP.pb.h"

#include <grpcpp/server.h>

namespace server::grpc_communication
{

class tinykvpp_service_impl_t final : public TinyKVPPService::Service
{
  public:
    explicit tinykvpp_service_impl_t(db::shared_ptr_t db);

    auto
    Put(grpc::ServerContext *pContext, const PutRequest *pRequest, PutResponse *pResponse) -> grpc::Status override;

    auto
    Get(grpc::ServerContext *pContext, const GetRequest *pRequest, GetResponse *pResponse) -> grpc::Status override;

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

static_assert(communication_strategy_t<grpc_communication_t>,
              "GRPCCommunication must satisfy CommunicationStrategy concept");

} // namespace server::grpc_communication
