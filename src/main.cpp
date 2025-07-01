#include <thread>
#include <exception>
#include <format>
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

template <typename TEntry>
[[nodiscard]] auto maybe_create_file_persistent_wal(const fs::path_t &walPath)
    -> std::optional<wal::wal_t<TEntry>>
{
    auto logStorage{wal::log::storage::log_storage_builder_t{}
                        .set_file_path(walPath)
                        .set_check_path_exists(false)
                        .build<TEntry>(wal::log_storage_type_k::file_based_persistent_k)};
    if (!logStorage.has_value())
    {
        spdlog::error(
            "maybe_create_file_persistent_wal: Unable to build a log stroage. path={}",
            walPath.c_str()
        );
        return std::nullopt;
    }

    auto log = wal::log::log_builder_t{}.build(std::move(logStorage.value()));
    if (!log.has_value())
    {
        spdlog::debug("maybe_create_file_persistent_wal: Unable to build a log");
        return std::nullopt;
    }

    auto maybeWal = wal::wal_builder_t<TEntry>{}.build(std::move(log.value()));
    if (maybeWal.has_value())
    {
        return std::make_optional(std::move(maybeWal.value()));
    }

    spdlog::error(
        "maybe_create_consensus_module: Unable to build WAL. Error={}",
        magic_enum::enum_name(maybeWal.error())
    );
    return std::nullopt;
}

[[nodiscard]] auto maybe_create_manifest(const fs::path_t &path)
    -> std::optional<db::manifest::manifest_t>
{
    auto maybeWal{maybe_create_file_persistent_wal<db::manifest::manifest_t::record_t>(path)};
    if (!maybeWal.has_value())
    {
        spdlog::error(
            "maybe_create_manifest: Unable to create a WAL for the manifest. path={}", path.c_str()
        );
        return std::nullopt;
    }
    return db::manifest::manifest_builder_t{}.build(path, std::move(maybeWal.value()));
}

[[nodiscard]] auto
maybe_create_consensus_module(config::shared_ptr_t pConfig, raft::node_config_t nodeConfig) noexcept
    -> std::optional<std::shared_ptr<raft::consensus_module_t>>
{
    if (pConfig->ServerConfig.id == 0)
    {
        spdlog::error("maybe_create_consensus_module: ID of the node should be positve integer");
        return std::nullopt;
    }

    if (pConfig->ServerConfig.peers.empty())
    {
        spdlog::error("maybe_create_consensus_module: List of node IPs can't be empty");
        return std::nullopt;
    }

    const auto raftWalPath{
        pConfig->DatabaseConfig.DatabasePath / std::format("raft_wal_{}", pConfig->ServerConfig.id)
    };
    auto maybeWal =
        maybe_create_file_persistent_wal<raft::consensus_module_t::wal_entry_t>(raftWalPath);
    if (!maybeWal.has_value())
    {
        spdlog::error("maybe_create_consensus_module: Unable to build WAL.");
        return std::nullopt;
    }

    std::vector<raft::raft_node_grpc_client_t> replicas;
    for (raft::id_t replicaId{1}; const auto &replicaIp : pConfig->ServerConfig.peers)
    {
        if (replicaId != pConfig->ServerConfig.id)
        {
            std::unique_ptr<RaftService::Stub> stub{RaftService::NewStub(
                grpc::CreateChannel(replicaIp, grpc::InsecureChannelCredentials())
            )};

            replicas.emplace_back(
                raft::node_config_t{.m_id = replicaId, .m_ip = replicaIp}, std::move(stub)
            );

            spdlog::info(
                "maybe_create_consensus_module: Emplacing replica into replicas "
                "vector. replicaId={} replicaIp={}",
                replicaId,
                replicaIp
            );
        }
        ++replicaId;
    }

    auto pConsensusModule = std::make_shared<raft::consensus_module_t>(
        nodeConfig,
        std::move(replicas),
        wal::make_shared<raft::consensus_module_t::wal_entry_t>(std::move(maybeWal.value()))
    );

    if (!pConsensusModule->init())
    {
        spdlog::error("maybe_create_consensus_module: Failed to initialize the consensus module");
        return std::nullopt;
    }

    return std::make_optional(std::move(pConsensusModule));
}

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

        cxxopts::Options options("tinykvpp", "A tiny database, powering big ideas");
        options.add_options()("c,config",
                                                                        "Path to JSON "
                                                                        "configuration of database",
                                                                        cxxopts::value<std::string>())(
                                                                        "help", "Print help");

        auto parsedOptions = options.parse(argc, argv);
        if ((parsedOptions.count("help") != 0U) || (parsedOptions.count("config") == 0U))
        {
            spdlog::info("{}", options.help());
            return EXIT_SUCCESS;
        }

        const auto configPath = parsedOptions["config"].as<std::string>();
        auto       configJson = loadConfigJson(configPath);
        validateConfigJson(configJson);

        configureLogging(configJson["logging"]["loggingLevel"].get<std::string>());

        auto pDbConfig = initializeDatabaseConfig(configJson, configPath);
        if (pDbConfig->WALConfig.storageType == wal::log_storage_type_k::undefined_k)
        {
            spdlog::error("Undefined WAL storage type");
            return EXIT_FAILURE;
        }

        // ==== Start: Build consensus module
        raft::node_config_t nodeConfig{
            .m_id = pDbConfig->ServerConfig.id,
            .m_ip = fmt::format("{}:{}", pDbConfig->ServerConfig.host, pDbConfig->ServerConfig.port)
        };

        // Start building gRPC server
        grpc::ServerBuilder grpcBuilder;

        // Listen on the current nodes host:port
        // TODO(lnikon): Drop insecure creds
        grpcBuilder.AddListeningPort(nodeConfig.m_ip, grpc::InsecureServerCredentials());

        std::shared_ptr<raft::consensus_module_t> pConsensusModule{nullptr};
        std::shared_ptr<db::db_t>                 pDatabase{nullptr};
        if (pDbConfig->DatabaseConfig.mode == db::db_mode_t::kReplicated)
        {
            if (auto maybeConsensusModule{maybe_create_consensus_module(pDbConfig, nodeConfig)};
                maybeConsensusModule.has_value())
            {
                pConsensusModule = std::move(maybeConsensusModule.value());
                grpcBuilder.RegisterService(
                    dynamic_cast<RaftService::Service *>(pConsensusModule.get())
                );
            }
            else
            {
                spdlog::debug("Main: Failed to create a consensus module. Exiting.");
                return EXIT_FAILURE;
            }
        }
        else
        {
            spdlog::info(
                "Main: Skipping creation of consensus module as database is not in {} "
                "mode",
                magic_enum::enum_name(db::db_mode_t::kReplicated)
            );
        }
        // ==== End: Build consensus module

        // ==== Start: Build WAL ====
        // Build the log storage
        const auto walPath{pDbConfig->DatabaseConfig.DatabasePath / pDbConfig->WALConfig.path};
        auto       logStorage{wal::log::storage::log_storage_builder_t{}
                            .set_file_path(walPath)
                            .set_consensus_module(pConsensusModule)
                            .build<wal::wal_entry_t>(pDbConfig->WALConfig.storageType)};
        if (!logStorage.has_value())
        {
            spdlog::error("Unable to build a log stroage. path={}", walPath.c_str());
            return EXIT_FAILURE;
        }

        // Build the log
        auto log = wal::log::log_builder_t{}.build(std::move(logStorage.value()));
        if (!log.has_value())
        {
            spdlog::debug("Main: Unable to build simple log");
            return EXIT_FAILURE;
        }

        // Build the WAL
        auto maybeWal = wal::wal_builder_t<wal::wal_entry_t>{}.build(std::move(log.value()));
        if (!maybeWal.has_value())
        {
            spdlog::error("Unable to build WAL. Error={}", magic_enum::enum_name(maybeWal.error()));
            return EXIT_FAILURE;
        }
        auto pWAL = wal::make_shared<wal::wal_entry_t>(std::move(maybeWal.value()));
        // ==== End: Build WAL ====

        // ==== Start: Build Manifest ====
        auto maybe_manifest = maybe_create_manifest(pDbConfig->manifest_path());
        if (!maybe_manifest.has_value())
        {
            spdlog::error("Main: Unable to create Manifest");
            return EXIT_FAILURE;
        }

        auto pManifest{
            std::make_shared<db::manifest::manifest_t>(std::move(maybe_manifest.value()))
        };
        // ==== End: Build Manifest ====

        // ==== Start: Build LSMTree
        auto pLSMTree = structures::lsmtree::lsmtree_builder_t{}.build(pDbConfig, pManifest, pWAL);
        if (!pLSMTree)
        {
            spdlog::error("Main: Unable to build LSMTree");
            return EXIT_FAILURE;
        }
        // ==== End: Build LSMTree

        // ==== Start: Build database
        pDatabase = db::make_shared(pDbConfig, pWAL, pManifest, pLSMTree, pConsensusModule);
        if (!pDatabase->open())
        {
            spdlog::error("Main: Unable to open the database");
            return EXIT_FAILURE;
        }
        // ==== End: Build database

        // Create KV service and add it into gRPC server
        auto kvService =
            std::make_unique<server::grpc_communication::tinykvpp_service_impl_t>(pDatabase);
        grpcBuilder.RegisterService(kvService.get());

        // Create gRPC server
        std::unique_ptr<grpc::Server> pServer{
            std::unique_ptr<grpc::Server>(grpcBuilder.BuildAndStart())
        };

        // Start consensus module and gRPC server
        auto serverThread = std::jthread([&pServer] { pServer->Wait(); });

        if (pDbConfig->DatabaseConfig.mode == db::db_mode_t::kReplicated && pConsensusModule)
        {
            pConsensusModule->setOnCommitCallback(
                [pDatabase](const raft::consensus_module_t::wal_entry_t &entry)
                {
                    if (!pDatabase)
                    {
                        spdlog::error("pDatabase is null, cannot commit entry");
                        return false;
                    }

                    db::db_t::record_t record;
                    std::stringstream  sstream{entry.payload()};
                    record.read(sstream);

                    return pDatabase->put(record, db::db_put_context_k::do_not_replicate_k);
                }
            );

            pConsensusModule->start();
        }

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

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Error: {}", e.what());
        return EXIT_FAILURE;
    }
}
