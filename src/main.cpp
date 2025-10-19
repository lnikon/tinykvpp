#include <thread>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <csignal>
#include <cstdlib>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <cxxopts.hpp>
#include <magic_enum/magic_enum.hpp>

#include "config.h"
#include "db.h"
#include "db_config.h"
#include "lsmtree.h"
#include "manifest/manifest.h"
#include "wal/common.h"
#include "memtable.h"
#include "raft/raft.h"
#include "server/grpc_server.h"

using tk_key_t = structures::memtable::memtable_t::record_t::key_t;
using tk_value_t = structures::memtable::memtable_t::record_t::value_t;

namespace
{

[[nodiscard]] auto maybe_create_manifest(const fs::path_t &path)
    -> std::optional<db::manifest::manifest_t>
{
    auto maybeWal =
        wal::wal_builder_t{}.set_file_path(path).build<db::manifest::manifest_t::record_t>(
            wal::log_storage_type_k::file_based_persistent_k
        );
    if (!maybeWal.has_value())
    {
        spdlog::error(
            "maybe_create_manifest: Unable to create a WAL for the manifest. path={}", path.c_str()
        );
        return std::nullopt;
    }
    return db::manifest::manifest_builder_t{}.build(path, std::move(maybeWal.value()));
}

[[nodiscard]] auto maybe_create_consensus_module(
    config::shared_ptr_t                  pConfig,
    consensus::node_config_t              nodeConfig,
    wal::shared_ptr_t<raft::v1::LogEntry> pWAL
) noexcept -> std::optional<std::shared_ptr<consensus::consensus_module_t>>
{
    if (pConfig->ServerConfig.id == 0)
    {
        spdlog::error("maybe_create_consensus_module: ID of the node should be a positive integer");
        return std::nullopt;
    }

    std::vector<consensus::raft_node_grpc_client_t> replicas;
    for (consensus::id_t replicaId{1}; const auto &replicaIp : pConfig->ServerConfig.peers)
    {
        if (replicaId != pConfig->ServerConfig.id)
        {
            std::unique_ptr<raft::v1::RaftService::Stub> stub{raft::v1::RaftService::NewStub(
                grpc::CreateChannel(replicaIp, grpc::InsecureChannelCredentials())
            )};

            replicas.emplace_back(
                consensus::node_config_t{.m_id = replicaId, .m_ip = replicaIp}, std::move(stub)
            );
        }
        ++replicaId;
    }

    auto pConsensusModule = std::make_shared<consensus::consensus_module_t>(
        pConfig, nodeConfig, std::move(replicas), pWAL
    );
    if (!pConsensusModule->init())
    {
        spdlog::error("maybe_create_consensus_module: Failed to initialize the consensus module");
        return std::nullopt;
    }

    return std::make_optional(std::move(pConsensusModule));
}

auto prepare_directory_structure(config::shared_ptr_t pConfig) -> bool
{
    // Create database directory
    if (!std::filesystem::exists(pConfig->DatabaseConfig.DatabasePath))
    {
        spdlog::info(
            "Creating database directory at {}", pConfig->DatabaseConfig.DatabasePath.c_str()
        );
        if (!std::filesystem::create_directory(pConfig->DatabaseConfig.DatabasePath))
        {
            spdlog::error(
                "Failed to create database directory at {}",
                pConfig->DatabaseConfig.DatabasePath.c_str()
            );
            return false;
        }
    }
    else
    {
        spdlog::info("Opening database at {}", pConfig->DatabaseConfig.DatabasePath.c_str());
    }

    // Create segments directory inside database directory
    const auto &segmentsPath{pConfig->datadir_path()};
    if (!std::filesystem::exists(segmentsPath))
    {
        spdlog::info("Creating segments directory at {}", segmentsPath.c_str());
        if (!std::filesystem::create_directory(segmentsPath))
        {
            spdlog::error("Failed to create segments directory at {}", segmentsPath.c_str());
            return false;
        }
    }

    return true;
}

} // namespace

std::atomic<bool> gShutdown{false};
static void       signalHandler(int sig) noexcept
{
    if (sig == SIGTERM || sig == SIGINT)
    {
        gShutdown.store(true);
    }
}

auto main(int argc, char *argv[]) -> int
{
    try
    {
        // Setup signal handlers
        if (std::signal(SIGTERM, signalHandler) == SIG_ERR)
        {
            spdlog::error("Unable to set signal handler for SIGTERM");
            return EXIT_FAILURE;
        }

        if (std::signal(SIGINT, signalHandler) == SIG_ERR)
        {
            spdlog::error("Unable to set signal handler for SIGINT");
            return EXIT_FAILURE;
        }

        // Setup options
        cxxopts::Options options("tinykvpp", "A tiny database, powering big ideas");
        options
            .add_options()("c,config", "Path to JSON configuration of database", cxxopts::value<std::string>())(
                "help", "Print help"
            );

        const auto &parsedOptions{options.parse(argc, argv)};
        if ((parsedOptions.count("help") != 0U) || (parsedOptions.count("config") == 0U))
        {
            spdlog::info(options.help());
            return EXIT_SUCCESS;
        }

        // Load the config
        const auto &configPath = parsedOptions["config"].as<std::string>();
        const auto &configJson = loadConfigJson(configPath);
        validateConfigJson(configJson);

        // Setup the logging
        configureLogging(configJson["logging"]["loggingLevel"].get<std::string>());

        // Setup database config
        auto pDbConfig = initializeDatabaseConfig(configJson, configPath);
        if (pDbConfig->WALConfig.storageType == wal::log_storage_type_k::undefined_k)
        {
            spdlog::error("Undefined WAL storage type");
            return EXIT_FAILURE;
        }

        // Setup directory structure for the database
        prepare_directory_structure(pDbConfig);

        // Build WAL
        const auto walPath{pDbConfig->DatabaseConfig.DatabasePath / pDbConfig->WALConfig.path};
        auto       maybeWal = wal::wal_builder_t{}.set_file_path(walPath).build<raft::v1::LogEntry>(
            pDbConfig->WALConfig.storageType
        );
        if (!maybeWal.has_value())
        {
            spdlog::error("Unable to build WAL");
            return EXIT_FAILURE;
        }
        auto pWAL = wal::make_shared<raft::v1::LogEntry>(std::move(maybeWal.value()));

        // Build consensus module
        consensus::node_config_t nodeConfig{
            .m_id = pDbConfig->ServerConfig.id,
            .m_ip = fmt::format("{}:{}", pDbConfig->ServerConfig.host, pDbConfig->ServerConfig.port)
        };

        // Listen on the current nodes host:port
        // TODO(lnikon): Drop insecure creds
        grpc::ServerBuilder grpcBuilder;
        grpcBuilder.AddListeningPort(nodeConfig.m_ip, grpc::InsecureServerCredentials());

        std::shared_ptr<consensus::consensus_module_t> pConsensusModule{nullptr};
        if (auto maybeConsensusModule{maybe_create_consensus_module(pDbConfig, nodeConfig, pWAL)};
            maybeConsensusModule.has_value())
        {
            pConsensusModule = std::move(maybeConsensusModule.value());
            grpcBuilder.RegisterService(
                dynamic_cast<raft::v1::RaftService::Service *>(pConsensusModule.get())
            );
        }
        else
        {
            spdlog::debug("Main: Failed to create a consensus module. Exiting.");
            return EXIT_FAILURE;
        }

        // Build Manifest
        auto maybe_manifest = maybe_create_manifest(pDbConfig->manifest_path());
        if (!maybe_manifest.has_value())
        {
            spdlog::error("Main: Unable to create Manifest");
            return EXIT_FAILURE;
        }
        auto pManifest{
            std::make_shared<db::manifest::manifest_t>(std::move(maybe_manifest.value()))
        };

        // Build LSMTree
        auto pLSMTree = structures::lsmtree::lsmtree_builder_t{}.build(pDbConfig, pManifest, pWAL);
        if (!pLSMTree)
        {
            spdlog::error("Main: Unable to build LSMTree");
            return EXIT_FAILURE;
        }

        // Build the database
        auto pDatabase = db::make_shared(pDbConfig, pManifest, pLSMTree, pConsensusModule);
        if (!pDatabase->start())
        {
            spdlog::error("Main: Unable to start the database");
            return EXIT_FAILURE;
        }

        // Create KV service and connect it to the gRPC server
        auto kvService =
            std::make_unique<server::grpc_communication::tinykvpp_service_impl_t>(pDatabase);
        grpcBuilder.RegisterService(kvService.get());

        // Create gRPC server
        std::unique_ptr<grpc::Server> pServer{
            std::unique_ptr<grpc::Server>(grpcBuilder.BuildAndStart())
        };

        // Start consensus module and gRPC server
        auto serverThread = std::jthread([&pServer] { pServer->Wait(); });

        // Start consensus module
        if (pConsensusModule)
        {
            pConsensusModule->start();
        }

        // SIGTERM/SIGINT received, shutdown services and servers
        while (!gShutdown)
        {
            std::this_thread::yield();
        }

        spdlog::debug("Node={} is requesting server shutdown", nodeConfig.m_id);
        pServer->Shutdown();

        spdlog::debug("Node={} is joining the server thread", nodeConfig.m_id);
        if (serverThread.joinable())
        {
            serverThread.join();
            spdlog::debug("Node={} joined the server thread", nodeConfig.m_id);
        }

        if (pConsensusModule)
        {
            pConsensusModule->stop();
        }

        pDatabase->stop();

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Error: {}", e.what());
        return EXIT_FAILURE;
    }
}
