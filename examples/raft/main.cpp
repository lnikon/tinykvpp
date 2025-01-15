#include <csignal>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "raft.h"

#include <grpcpp/server.h>

#include <cxxopts.hpp>

#include <spdlog/spdlog.h>

std::condition_variable gCv;

static void signalHandler(int sig)
{
    if (sig == SIGTERM || sig == SIGINT)
    {
        gCv.notify_all();
    }
}

auto main(int argc, char *argv[]) -> int
{
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    cxxopts::Options options("raft");
    options.add_options()("id", "id of the node", cxxopts::value<id_t>())(
        "nodes", "ip addresses of replicas in a correct order", cxxopts::value<std::vector<raft::ip_t>>());

    auto parsedOptions = options.parse(argc, argv);
    if ((parsedOptions.count("help") != 0U) || (parsedOptions.count("id") == 0U) ||
        (parsedOptions.count("nodes") == 0U))
    {
        spdlog::info("{}", options.help());
        return EXIT_SUCCESS;
    }

    auto nodeId = parsedOptions["id"].as<id_t>();
    if (nodeId == 0)
    {
        spdlog::error("ID of the node should be positve integer");
        return EXIT_FAILURE;
    }

    auto nodeIps = parsedOptions["nodes"].as<std::vector<raft::ip_t>>();
    if (nodeIps.empty())
    {
        spdlog::error("List of node IPs can't be empty");
        return EXIT_FAILURE;
    }

    std::vector<raft::node_client_t> replicas;
    for (raft::id_t replicaId{1}; const auto &replicaIp : nodeIps)
    {
        if (replicaId != nodeId)
        {
            std::unique_ptr<RaftService::Stub> stub{
                RaftService::NewStub(grpc::CreateChannel(replicaIp, grpc::InsecureChannelCredentials()))};

            replicas.emplace_back(raft::node_config_t{.m_id = replicaId, .m_ip = replicaIp}, std::move(stub));
        }

        ++replicaId;
    }

    raft::consensus_module_t consensusModule({.m_id = nodeId, .m_ip = nodeIps[nodeId - 1]}, std::move(replicas));
    if (!consensusModule.init())
    {
        spdlog::error("Failed to initialize the state machine");
        return EXIT_FAILURE;
    }

    spdlog::set_level(spdlog::level::debug);
    consensusModule.start();

    std::mutex                   mtx;
    std::unique_lock<std::mutex> lock(mtx);
    gCv.wait(lock);

    consensusModule.stop();

    return EXIT_SUCCESS;
}
