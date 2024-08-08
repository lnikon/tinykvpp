#include "config.h"
#include "db.h"
#include "memtable.h"
#include <cstdlib>
#include <exception>
#include <fstream>

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <cxxopts.hpp>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>

using tk_key_t = structures::memtable::memtable_t::record_t::key_t;
using tk_value_t = structures::memtable::memtable_t::record_t::value_t;

using nlohmann::json;
using nlohmann::json_schema::json_validator;

// The schema is defined based upon a string literal
static json database_config_schema = R"(
{
  "$id": "https://json-schema.hyperjump.io/schema",
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$title": "Schema for tinykvpp's JSON config",
  "type": "object",
  "properties": {
    "logging": {
      "type": "object",
      "properties": {
        "loggingLevel": {
          "$ref": "#/$defs/loggingLevel"
        }
      },
      "required": [
        "loggingLevel"
      ]
    },
    "database": {
      "type": "object",
      "properties": {
        "path": {
          "type": "string"
        },
        "walFilename": {
          "type": "string"
        },
        "manifestFilenamePrefix": {
          "type": "string"
        }
      },
      "required": [
        "path",
        "walFilename",
        "manifestFilenamePrefix"
      ]
    },
    "lsmtree": {
      "type": "object",
      "properties": {
        "memtableFlushThreshold": {
          "type": "number"
        },
        "levelZeroCompaction": {
          "$ref": "#/$defs/compaction"
        },
        "levelNonZeroCompaction": {
          "$ref": "#/$defs/compaction"
        }
      },
      "required": [
        "memtableFlushThreshold",
        "levelZeroCompaction",
        "levelNonZeroCompaction"
      ]
    }
  },
  "required": [
    "logging",
    "database",
    "lsmtree"
  ],
  "$defs": {
    "loggingLevel": {
      "type": "string",
      "enum": [
        "info",
        "debug"
      ]
    },
    "compactionStrategy": {
      "type": "string",
      "enum": [
        "levelled",
        "tiered"
      ]
    },
    "compaction": {
      "type": "object",
      "properties": {
        "compactionStrategy": {
          "$ref": "#/$defs/compactionStrategy"
        },
        "compactionThreshold": {
          "type": "number"
        }
      },
      "required": [
        "compactionStrategy",
        "compactionThreshold"
      ]
    }
  }
}

)"_json;

using json = nlohmann::json;

auto loadConfigJson(const std::string &configPath) -> json
{
    std::fstream configStream(configPath, std::fstream::in);
    if (!configStream.is_open())
    {
        throw std::runtime_error("Unable to open config file: " + configPath);
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
        throw std::runtime_error("Database config validation failed. Error: " + std::string(e.what()));
    }
}

void configureLogging(const std::string &loggingLevel)
{
    if (loggingLevel == SPDLOG_LEVEL_NAME_INFO)
    {
        spdlog::set_level(spdlog::level::info);
    }
    else if (loggingLevel == SPDLOG_LEVEL_NAME_DEBUG)
    {
        spdlog::set_level(spdlog::level::debug);
    }
    else
    {
        throw std::runtime_error("Unsupported logging level: " + loggingLevel);
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
        dbConfig->DatabaseConfig.WalFilename = configJson["database"]["walFilename"].get<std::string>();
    }

    if (configJson["database"].contains("manifestFilenamePrefix"))
    {
        dbConfig->DatabaseConfig.ManifestFilenamePrefix =
            configJson["database"]["manifestFilenamePrefix"].get<std::string>();
    }

    return dbConfig;
}

void loadLSMTreeConfig(const json &lsmtreeConfig, config::shared_ptr_t dbConfig, const std::string &configPath)
{
    if (lsmtreeConfig.contains("memtableFlushThreshold"))
    {
        dbConfig->LSMTreeConfig.DiskFlushThresholdSize = lsmtreeConfig["memtableFlushThreshold"].get<uint64_t>();
    }
    else
    {
        throw std::runtime_error("\"memtableFlushThreshold\" is not specified in config: " + configPath);
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
            throw std::runtime_error("\"levelZeroCompaction.compactionStrategy\" is not specified in config: " +
                                     configPath);
        }

        if (levelZeroCompaction.contains("compactionThreshold"))
        {
            dbConfig->LSMTreeConfig.LevelZeroCompactionThreshold =
                levelZeroCompaction["compactionThreshold"].get<std::uint64_t>();
        }
        else
        {
            throw std::runtime_error("\"levelZeroCompaction.compactionThreshold\" is not specified in config: " +
                                     configPath);
        }
    }
    else
    {
        throw std::runtime_error("\"levelZeroCompaction\" is not specified in config: " + configPath);
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
            throw std::runtime_error("\"levelNonZeroCompaction.compactionStrategy\" is not specified in config: " +
                                     configPath);
        }

        if (levelNonZeroCompaction.contains("compactionThreshold"))
        {
            dbConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold =
                levelNonZeroCompaction["compactionThreshold"].get<std::uint64_t>();
        }
        else
        {
            throw std::runtime_error("\"levelNonZeroCompaction.compactionThreshold\" is not specified in config: " +
                                     configPath);
        }
    }
    else
    {
        throw std::runtime_error("\"levelNonZeroCompaction\" is not specified in config: " + configPath);
    }
}

auto initializeDatabase(const json &configJson, const std::string &configPath) -> config::shared_ptr_t
{
    auto dbConfig = loadDatabaseConfig(configJson);

    if (configJson.contains("lsmtree"))
    {
        loadLSMTreeConfig(configJson["lsmtree"], dbConfig, configPath);
    }
    else
    {
        throw std::runtime_error("\"lsmtree\" is not specified in config: " + configPath);
    }

    return dbConfig;
}

auto main(int argc, char *argv[]) -> int
{
    try
    {

        cxxopts::Options options("tinykvpp", "A tiny database, powering big ideas");
        options.add_options()("c,config", "Path to JSON configuration of database", cxxopts::value<std::string>())(
            "help", "Print help");

        auto parsedOptions = options.parse(argc, argv);
        if ((parsedOptions.count("help") != 0U) || (parsedOptions.count("config") == 0U))
        {
            spdlog::info("{}", options.help());
            return EXIT_SUCCESS;
        }

        const auto configPath = parsedOptions["config"].as<std::string>();
        auto configJson = loadConfigJson(configPath);

        json_validator validator;
        validator.set_root_schema(database_config_schema);
        validateConfigJson(configJson, validator);

        configureLogging(configJson["logging"]["loggingLevel"].get<std::string>());

        auto dbConfig = initializeDatabase(configJson, configPath);
        auto db = db::db_t(dbConfig);
        if (!db.open())
        {
            spdlog::error("Unable to open the database");
            return EXIT_FAILURE;
        }

        // Example usage of the database
        auto key = tk_key_t{"goodbye"};
        auto value = tk_value_t{"internet"};
        db.put(key, value);

        auto record = db.get(key);
        if (record.has_value())
        {
            spdlog::info("Found record key={} and value={}", record->m_key.m_key, record->m_value.m_value);
        }
        else
        {
            spdlog::error("Unable to find record with key={}", key.m_key);
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Error: {}", e.what());
        return EXIT_FAILURE;
    }
}
