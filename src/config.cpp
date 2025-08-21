#include <fstream>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <cxxopts.hpp>
#include <magic_enum/magic_enum.hpp>

#include "config.h"
#include "db_config.h"
#include "wal/common.h"

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
        }
      },
      "required": ["path", "manifestFilenamePrefix"]
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
      "enum": ["inMemory", "persistent", "replicated"]
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

auto loadConfigJson(const std::string &configPath) -> json
{
    std::fstream configStream(configPath, std::fstream::in);
    if (!configStream.is_open())
    {
        throw std::runtime_error(fmt::format("Unable to open config file: %s", configPath));
    }
    return json::parse(configStream);
}

void validateConfigJson(const json &configJson)
{
    try
    {
        json_validator validator;
        validator.set_root_schema(database_config_schema);
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

void loadLSMTreeConfig(
    const json &lsmtreeConfig, config::shared_ptr_t dbConfig, const std::string &configPath
)
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
            throw std::runtime_error(
                "\"levelZeroCompaction.compactionStrategy\" is not specified "
                "in config: " +
                configPath
            );
        }

        if (levelZeroCompaction.contains("compactionThreshold"))
        {
            dbConfig->LSMTreeConfig.LevelZeroCompactionThreshold =
                levelZeroCompaction["compactionThreshold"].get<std::uint64_t>();
        }
        else
        {
            throw std::runtime_error(
                "\"levelZeroCompaction.compactionThreshold\" is not specified "
                "in config: " +
                configPath
            );
        }
    }
    else
    {
        throw std::runtime_error(
            "\"levelZeroCompaction\" is not specified in config: " + configPath
        );
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
            throw std::runtime_error(
                "\"levelNonZeroCompaction.compactionStrategy\" is not "
                "specified in config: " +
                configPath
            );
        }

        if (levelNonZeroCompaction.contains("compactionThreshold"))
        {
            dbConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold =
                levelNonZeroCompaction["compactionThreshold"].get<std::uint64_t>();
        }
        else
        {
            throw std::runtime_error(
                "\"levelNonZeroCompaction.compactionThreshold\" is not "
                "specified in config: " +
                configPath
            );
        }
    }
    else
    {
        throw std::runtime_error(
            "\"levelNonZeroCompaction\" is not specified in config: " + configPath
        );
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
