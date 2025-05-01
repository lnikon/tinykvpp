#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <csignal>
#include <thread>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <cxxopts.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <magic_enum/magic_enum.hpp>

#include "config.h"
#include "db.h"
#include "db_config.h"
#include "wal/common.h"
#include "memtable.h"
#include "raft/raft.h"
#include "server/grpc_server.h"

using tk_key_t = structures::memtable::memtable_t::record_t::key_t;
using tk_value_t = structures::memtable::memtable_t::record_t::value_t;

using nlohmann::json;
using nlohmann::json_schema::json_validator;

// JSON schema for the configuration file
static const json database_config_schema = R"(
{
  "$id": "https://json-schema.hyperjump.io/schema",
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$title": "Schema for frankie's JSON config",
  "type": "object",
  "properties": {
    "logging": {
      "type": "object",
      "properties": {
        "loggingLevel": {
          "$ref": "#/$defs/loggingLevel"
        }
      },
      "required": ["loggingLevel"]
    },
    "database": {
      "type": "object",
      "description": "Core database configuration settings",
      "properties": {
        "path": {
          "type": "string",
          "description": "Database storage directory path"
        },
        "manifestFilenamePrefix": {
          "type": "string",
          "description": "Prefix for manifest files"
        },
        "mode": {
          "$ref": "#/$defs/mode",
          "description": "Specifies wheter the database will be embedded, run in server-client mode, or replicated"
        }
      },
      "required": ["path", "manifestFilenamePrefix", "mode"]
    },
    "wal": {
      "type": "object",
      "description": "WAL configuration",
      "properties": {
        "enable": {
          "type": "boolean",
          "description": "Enable/disable the WAL"
        },
        "storageType": {
          "$ref": "#/$defs/walStorageType"
        },
        "filename": {
          "type": "string",
          "description": "Write-Ahead Log filename"
        }
      },
      "required": ["enable", "storageType", "filename"]
    },
    "lsm": {
      "type": "object",
      "properties": {
        "flushThreshold": {
          "type": "integer",
          "description": "The threshold of bytes at which the memtable should be flushed",
          "minimum": 1
        },
        "levelZeroCompaction": {
          "$ref": "#/$defs/compaction"
        },
        "levelNonZeroCompaction": {
          "$ref": "#/$defs/compaction"
        }
      },
      "required": [
        "flushThreshold",
        "maximumLevels",
        "levelZeroCompaction",
        "levelNonZeroCompaction"
      ]
    },
    "server": {
      "type": "object",
      "description": "Server configuration settings",
      "properties": {
        "transport": {
          "$ref": "#/$defs/serverTransport"
        },
        "host": {
          "type": "string",
          "description": "Server host address"
        },
        "port": {
          "type": "integer",
          "description": "Server port number",
          "minimum": 1024,
          "maximum": 65535
        },
        "id": {
          "type": "integer",
          "description": "ID of the node",
          "minimum": 1
        },
        "peers": {
          "type": "array",
          "description": "Array of IPv4 addresses of peers",
          "items": {
            "type": "string"
          }
        }
      },
      "required": ["transport", "host", "port", "id", "peers"]
    }
  },
  "required": ["database", "wal", "lsm", "server"],
  "$defs": {
    "mode": {
      "type": "string",
      "enum": ["embedded", "standalone", "replicated"]
    },
    "serverTransport": {
      "type": "string",
      "enum": ["grpc", "tcp"]
    },
    "loggingLevel": {
      "type": "string",
      "enum": ["info", "debug", "trace", "off"]
    },
    "compactionStrategy": {
      "type": "string",
      "enum": ["levelled", "tiered"]
    },
    "walStorageType": {
      "type": "string",
      "enum": ["inMemory", "persistent"]
    },
    "compaction": {
      "type": "object",
      "properties": {
        "compactionStrategy": {
          "$ref": "#/$defs/compactionStrategy"
        },
        "compactionThreshold": {
          "type": "integer",
          "description": "Number of files that trigger compaction",
          "minimum": 1
        }
      },
      "required": ["compactionStrategy", "compactionThreshold"]
    }
  }
})"_json;

std::atomic<bool> gShutdown{false};

static void signalHandler(int sig) noexcept
{
    if (sig == SIGTERM || sig == SIGINT)
    {
        gShutdown.store(true);
    }
}

using json = nlohmann::json;

auto loadConfigJson(const std::string &configPath) -> json
{
    std::fstream configStream(configPath, std::fstream::in);
    if (!configStream.is_open())
    {
        throw std::runtime_error(fmt::format("Unable to open config file: %s", configPath));
    }
    return json::parse(configStream);
}

void validateConfigJson(const json &configJson, json_validator &validator)
{
    try
    {
        validator.validate(configJson);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(fmt::format("Config validation failed: {}", e.what()));
    }
}

void configureLogging(const std::string &loggingLevel)
{
    // spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
    if (loggingLevel == SPDLOG_LEVEL_NAME_INFO)
    {
        spdlog::set_level(spdlog::level::info);
    }
    else if (loggingLevel == SPDLOG_LEVEL_NAME_DEBUG)
    {
        spdlog::set_level(spdlog::level::debug);
    }
    else if (loggingLevel == SPDLOG_LEVEL_NAME_TRACE)
    {
        spdlog::set_level(spdlog::level::trace);
    }
    else if (loggingLevel == SPDLOG_LEVEL_NAME_OFF)
    {
        spdlog::set_level(spdlog::level::off);
    }
    else
    {
        throw std::runtime_error(fmt::format("Unknown logging level: %s", loggingLevel));
    }
}

auto loadDatabaseConfig(const json &configJson) -> config::shared_ptr_t
{
    auto dbConfig = config::make_shared();

    if (configJson["database"].contains("path"))
    {
        dbConfig->DatabaseConfig.DatabasePath = configJson["database"]["path"].get<std::string>();
    }

    if (configJson["database"].contains("walFilename"))
    {
        dbConfig->DatabaseConfig.WalFilename =
            configJson["database"]["walFilename"].get<std::string>();
    }

    if (configJson["database"].contains("manifestFilenamePrefix"))
    {
        dbConfig->DatabaseConfig.ManifestFilenamePrefix =
            configJson["database"]["manifestFilenamePrefix"].get<std::string>();
    }

    if (configJson["database"].contains("mode"))
    {
        dbConfig->DatabaseConfig.mode =
            db::from_string(configJson["database"]["mode"].get<std::string>());
    }

    return dbConfig;
}

void loadWALConfig(const json &walConfig, config::shared_ptr_t dbConfig)
{
    if (walConfig.contains("enable"))
    {
        dbConfig->WALConfig.enable = walConfig["enable"].get<bool>();
    }
    else
    {
        throw std::runtime_error("\"wal.enable\" is not specified in config");
    }

    if (walConfig.contains("storageType"))
    {
        dbConfig->WALConfig.storageType =
            wal::from_string(walConfig["storageType"].get<std::string>());
    }
    else
    {
        throw std::runtime_error("\"wal.storageType\" is not specified in config");
    }

    if (walConfig.contains("filename"))
    {
        dbConfig->WALConfig.path = walConfig["filename"].get<std::string>();
    }
    else
    {
        throw std::runtime_error("\"wal.filename\" is not specified in config");
    }
}

void loadLSMTreeConfig(const json          &lsmtreeConfig,
                       config::shared_ptr_t dbConfig,
                       const std::string   &configPath)
{
    if (lsmtreeConfig.contains("flushThreshold"))
    {
        dbConfig->LSMTreeConfig.DiskFlushThresholdSize =
            lsmtreeConfig["flushThreshold"].get<uint64_t>();
    }
    else
    {
        throw std::runtime_error("\"flushThreshold\" is not specified in config: " + configPath);
    }

    if (lsmtreeConfig.contains("levelZeroCompaction"))
    {
        const auto &levelZeroCompaction = lsmtreeConfig["levelZeroCompaction"];
        if (levelZeroCompaction.contains("compactionStrategy"))
        {
            dbConfig->LSMTreeConfig.LevelZeroCompactionStrategy =
                levelZeroCompaction["compactionStrategy"].get<std::string>();
        }
        else
        {
            throw std::runtime_error("\"levelZeroCompaction.compactionStrategy\" is not specified "
                                     "in config: " +
                                     configPath);
        }

        if (levelZeroCompaction.contains("compactionThreshold"))
        {
            dbConfig->LSMTreeConfig.LevelZeroCompactionThreshold =
                levelZeroCompaction["compactionThreshold"].get<std::uint64_t>();
        }
        else
        {
            throw std::runtime_error("\"levelZeroCompaction.compactionThreshold\" is not specified "
                                     "in config: " +
                                     configPath);
        }
    }
    else
    {
        throw std::runtime_error("\"levelZeroCompaction\" is not specified in config: " +
                                 configPath);
    }

    if (lsmtreeConfig.contains("levelNonZeroCompaction"))
    {
        const auto &levelNonZeroCompaction = lsmtreeConfig["levelNonZeroCompaction"];
        if (levelNonZeroCompaction.contains("compactionStrategy"))
        {
            dbConfig->LSMTreeConfig.LevelNonZeroCompactionStrategy =
                levelNonZeroCompaction["compactionStrategy"].get<std::string>();
        }
        else
        {
            throw std::runtime_error("\"levelNonZeroCompaction.compactionStrategy\" is not "
                                     "specified in config: " +
                                     configPath);
        }

        if (levelNonZeroCompaction.contains("compactionThreshold"))
        {
            dbConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold =
                levelNonZeroCompaction["compactionThreshold"].get<std::uint64_t>();
        }
        else
        {
            throw std::runtime_error("\"levelNonZeroCompaction.compactionThreshold\" is not "
                                     "specified in config: " +
                                     configPath);
        }
    }
    else
    {
        throw std::runtime_error("\"levelNonZeroCompaction\" is not specified in config: " +
                                 configPath);
    }
}

auto loadServerConfig(const json &configJson, config::shared_ptr_t dbConfig)
{
    if (configJson.contains("host"))
    {
        dbConfig->ServerConfig.host = configJson["host"].get<std::string>();
    }
    else
    {
        throw std::runtime_error("\"host\" is not specified in the config");
    }

    if (configJson.contains("port"))
    {
        dbConfig->ServerConfig.port = configJson["port"].get<uint32_t>();
    }
    else
    {
        throw std::runtime_error("\"port\" is not specified in the config");
    }

    if (configJson.contains("transport"))
    {
        dbConfig->ServerConfig.transport = configJson["transport"].get<std::string>();
    }
    else
    {
        throw std::runtime_error("\"transport\" is not specified in the config");
    }

    if (configJson.contains("id"))
    {
        dbConfig->ServerConfig.id = configJson["id"].get<std::uint32_t>();
    }
    else
    {
        throw std::runtime_error("\"id\" is not specified in the config");
    }

    if (configJson.contains("peers"))
    {
        dbConfig->ServerConfig.peers = configJson["peers"].get<std::vector<std::string>>();
    }
    else
    {
        throw std::runtime_error("\"id\" is not specified in the config");
    }
}

auto initializeDatabaseConfig(const json &configJson, const std::string &configPath)
    -> config::shared_ptr_t
{
    auto dbConfig = loadDatabaseConfig(configJson);

    if (configJson.contains("lsm"))
    {
        loadLSMTreeConfig(configJson["lsm"], dbConfig, configPath);
    }
    else
    {
        throw std::runtime_error("\"lsm\" is not specified in config: " + configPath);
    }

    if (configJson.contains("wal"))
    {
        loadWALConfig(configJson["wal"], dbConfig);
    }
    else
    {
        throw std::runtime_error("\"wal\" is not specified in config: " + configPath);
    }

    if (configJson.contains("server"))
    {
        loadServerConfig(configJson["server"], dbConfig);
    }
    else
    {
        throw std::runtime_error("\"server\" is not specified in config: " + configPath);
    }

    return dbConfig;
}

[[nodiscard]] auto maybe_create_consensus_module(config::shared_ptr_t pConfig,
                                                 raft::node_config_t  nodeConfig) noexcept
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

    // Path to Raft WAL
    // TODO(lnikon): "_RAFT" should be configurable.
    const auto raftWalPath{pConfig->DatabaseConfig.DatabasePath / "raft_wal"};

    // Consensus module and its log
    auto logStorage{wal::log::storage::log_storage_builder_t{}
                        .set_file_path(raftWalPath)
                        .set_check_path_exists(false)
                        .build<raft::consensus_module_t::wal_entry_t>(
                            wal::log_storage_type_k::file_based_persistent_k)};
    if (!logStorage.has_value())
    {
        spdlog::error(
            "maybe_create_consensus_module: Unable to build a log stroage for Raft. path={}",
            raftWalPath.c_str());
        return std::nullopt;
    }

    auto log = wal::log::log_builder_t{}.build(std::move(logStorage.value()));
    if (!log.has_value())
    {
        spdlog::debug("maybe_create_consensus_module: Unable to build a log for Raft");
        return std::nullopt;
    }

    raft::consensus_module_t::wal_ptr_t raftWal{nullptr};
    if (auto maybeWal = wal::wal_builder_t<raft::consensus_module_t::wal_entry_t>{}.build(
            std::move(log.value()));
        maybeWal.has_value())
    {
        raftWal =
            wal::make_shared<raft::consensus_module_t::wal_entry_t>(std::move(maybeWal.value()));
    }
    else
    {
        spdlog::error("maybe_create_consensus_module: Unable to build WAL. Error={}",
                      magic_enum::enum_name(maybeWal.error()));
        return std::nullopt;
    }

    // Prepare config for replicas
    std::vector<raft::raft_node_grpc_client_t> replicas;
    for (raft::id_t replicaId{1}; const auto &replicaIp : pConfig->ServerConfig.peers)
    {
        if (replicaId != pConfig->ServerConfig.id)
        {
            std::unique_ptr<RaftService::Stub> stub{RaftService::NewStub(
                grpc::CreateChannel(replicaIp, grpc::InsecureChannelCredentials()))};

            replicas.emplace_back(raft::node_config_t{.m_id = replicaId, .m_ip = replicaIp},
                                  std::move(stub));

            spdlog::info("maybe_create_consensus_module: Emplacing replica into replicas "
                         "vector. replicaId={} replicaIp={}",
                         replicaId,
                         replicaIp);
        }

        ++replicaId;
    }

    // Create consensus module
    auto pConsensusModule =
        std::make_shared<raft::consensus_module_t>(nodeConfig, std::move(replicas), raftWal);
    if (!pConsensusModule->init())
    {
        spdlog::error("maybe_create_consensus_module: Failed to initialize the consensus module");
        return std::nullopt;
    }

    return std::make_optional(std::move(pConsensusModule));
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
                              "Path to JSON configuration of database",
                              cxxopts::value<std::string>())("help", "Print help");

        auto parsedOptions = options.parse(argc, argv);
        if ((parsedOptions.count("help") != 0U) || (parsedOptions.count("config") == 0U))
        {
            spdlog::info("{}", options.help());
            return EXIT_SUCCESS;
        }

        const auto configPath = parsedOptions["config"].as<std::string>();
        auto       configJson = loadConfigJson(configPath);

        json_validator validator;
        validator.set_root_schema(database_config_schema);
        validateConfigJson(configJson, validator);

        configureLogging(configJson["logging"]["loggingLevel"].get<std::string>());

        auto pDbConfig = initializeDatabaseConfig(configJson, configPath);
        if (pDbConfig->WALConfig.storageType == wal::log_storage_type_k::undefined_k)
        {
            spdlog::error("Undefined WAL storage type");
            return EXIT_FAILURE;
        }

        // Create current nodes config
        raft::node_config_t nodeConfig{
            .m_id = pDbConfig->ServerConfig.id,
            .m_ip =
                fmt::format("{}:{}", pDbConfig->ServerConfig.host, pDbConfig->ServerConfig.port)};

        // Start building gRPC server
        grpc::ServerBuilder grpcBuilder;

        // Listen on the current nodes host:port
        // TODO(lnikon): Drop insecure creds
        grpcBuilder.AddListeningPort(nodeConfig.m_ip, grpc::InsecureServerCredentials());

        std::shared_ptr<raft::consensus_module_t> pConsensusModule{nullptr};
        if (pDbConfig->DatabaseConfig.mode == db::db_mode_t::kReplicated)
        {
            if (auto maybeConsensusModule{maybe_create_consensus_module(pDbConfig, nodeConfig)};
                maybeConsensusModule.has_value())
            {
                pConsensusModule = std::move(maybeConsensusModule.value());
                grpcBuilder.RegisterService(
                    dynamic_cast<RaftService::Service *>(pConsensusModule.get()));
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
                "Main: Skipping creation of consensus module as database is not in {} mode",
                magic_enum::enum_name(db::db_mode_t::kReplicated));
        }

        // ==== Start: Build WAL ====
        // Build the log storage
        const auto walPath{pDbConfig->DatabaseConfig.DatabasePath / pDbConfig->WALConfig.path};

        auto logStorage{wal::log::storage::log_storage_builder_t{}
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

        // Build the  WAL
        wal::shared_ptr_t<wal::wal_entry_t> wal{nullptr};
        if (auto maybeWal = wal::wal_builder_t<wal::wal_entry_t>{}.build(std::move(log.value()));
            maybeWal.has_value())
        {
            wal = wal::make_shared<wal::wal_entry_t>(std::move(maybeWal.value()));
        }
        else
        {
            spdlog::error("Unable to build WAL. Error={}", magic_enum::enum_name(maybeWal.error()));
            return EXIT_FAILURE;
        }
        // ==== End: Build WAL ====

        auto pDatabase = db::make_shared(pDbConfig, std::move(wal));
        if (!pDatabase->open())
        {
            spdlog::error("Unable to open the database");
            return EXIT_FAILURE;
        }

        // Create KV service and add it into gRPC server
        auto kvService =
            std::make_unique<server::grpc_communication::tinykvpp_service_impl_t>(pDatabase);
        grpcBuilder.RegisterService(kvService.get());

        // Create gRPC server
        std::unique_ptr<grpc::Server> pServer{
            std::unique_ptr<grpc::Server>(grpcBuilder.BuildAndStart())};

        // Start consensus module and gRPC server
        auto serverThread = std::jthread([&pServer] { pServer->Wait(); });

        if (pDbConfig->DatabaseConfig.mode == db::db_mode_t::kReplicated && pConsensusModule)
        {
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
