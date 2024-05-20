#include <config/config.h>
#include <db/db.h>
#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <spdlog/spdlog.h>

#include "schemas/TinyKVPP.grpc.pb.h"
#include "schemas/TinyKVPP.pb.h"
#include <structures/lsmtree/lsmtree_types.h>

class TinyKVPPServiceImpl final : public TinyKVPPService::Service
{
   public:
    TinyKVPPServiceImpl(db::db_t &db)
        : m_db(db)
    {
    }

    grpc::Status Put(grpc::ServerContext *pContext,
                     const PutRequest *pRequest,
                     PutResponse *pResponse)
    {
        m_db.put(structures::lsmtree::key_t{pRequest->key()},
                 structures::lsmtree::value_t{pRequest->value()});
        pResponse->set_status(std::string("status message not implemented"));
        return grpc::Status::OK;
    }

    grpc::Status Get(grpc::ServerContext *pContext,
                     const GetRequest *pRequest,
                     GetResponse *pResponse)
    {
        const auto &record =
            m_db.get(structures::lsmtree::key_t{pRequest->key()});
        if (record)
        {
            pResponse->set_value(
                std::get<std::string>(record.value().m_value.m_value));
        }
        return grpc::Status::OK;
    }

   private:
    db::db_t &m_db;
};

void RunServer(db::db_t &db)
{
    static constexpr const std::string serverAddress("0.0.0.0:50051");
    TinyKVPPServiceImpl service(db);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("Server listening on {}", serverAddress);
    server->Wait();
}

int main(int argc, char *argv[])
{
    auto pConfig = config::make_shared();
    pConfig->LSMTreeConfig.DiskFlushThresholdSize = 10;
    db::db_t db(pConfig);
    if (!db.open())
    {
        std::cerr << "Unable to open the database" << std::endl;
    }

    RunServer(db);

    int* a = nullptr;
    *a = 4;
    return 0;
}
