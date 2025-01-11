#include <random>

#include "raft.h"

#include <grpcpp/server.h>

#include <cxxopts.hpp>

#include <spdlog/spdlog.h>

auto main(int argc, char *argv[]) -> int
{
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

    raft::consensus_module_t consensusModule(nodeId, nodeIps);
    if (!consensusModule.init())
    {
        spdlog::error("Failed to initialize the state machine");
        return EXIT_FAILURE;
    }

    consensusModule.start();

    /*cm.stop();*/

    return EXIT_SUCCESS;
}
