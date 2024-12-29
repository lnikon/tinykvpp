#include <random>

#include "raft.h"

#include <grpcpp/server.h>

#include <cxxopts.hpp>

#include <spdlog/spdlog.h>

auto generateRandomTimeout() -> int
{
    const int minTimeout{150};
    const int maxTimeout{300};

    std::random_device              randomDevice;
    std::mt19937                    gen(randomDevice());
    std::uniform_int_distribution<> dist(minTimeout, maxTimeout);

    return dist(gen);
}

int main(int argc, char *argv[])
{
    cxxopts::Options options("raft");
    options.add_options()("id", "id of the node", cxxopts::value<ID>())(
        "nodes", "ip addresses of replicas in a correct order", cxxopts::value<std::vector<IP>>());

    auto parsedOptions = options.parse(argc, argv);
    if ((parsedOptions.count("help") != 0U) || (parsedOptions.count("id") == 0U) ||
        (parsedOptions.count("nodes") == 0U))
    {
        spdlog::info("{}", options.help());
        return EXIT_SUCCESS;
    }

    auto nodeId = parsedOptions["id"].as<ID>();
    auto nodeIps = parsedOptions["nodes"].as<std::vector<IP>>();
    for (auto ip : nodeIps)
    {
        spdlog::info(ip);
    }

    spdlog::info("nodeIps.size()={}", nodeIps.size());

    if (nodeId == 0)
    {
        spdlog::error("ID of the node should be positve integer");
        return EXIT_FAILURE;
    }

    if (nodeIps.empty())
    {
        spdlog::error("List of node IPs can't be empty");
        return EXIT_FAILURE;
    }

    ConsensusModule cm(nodeId, nodeIps);
    if (!cm.init())
    {
        spdlog::error("Failed to initialize the state machine");
        return EXIT_FAILURE;
    }

    cm.start();

    cm.stop();

    return EXIT_SUCCESS;
}
