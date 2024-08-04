#include "config.h"
#include "db.h"
#include "memtable.h"
#include <cstdlib>
#include <exception>
#include <iostream>
#include <fstream>

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <cxxopts.hpp>
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
        "database": {
            "type": "object",
            "properties": {
                "path": {
                    "type": "string"
                },
                "walFilename": {
                    "type": "string"
                },
                "manifileFilenamePrefix": {
                    "type": "string"
                }
            }
        },
        "lsmtree": {
            "type": "object",
            "properties": {
                "memtableFlushThreashold": {
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
                "memtableFlushThreashold",
                "levelZeroCompaction",
                "levelNonZeroCompaction"
            ]
        }
    },
    "required": [
        "database",
        "lsmtree"
    ],
    "$defs": {
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
                "compactionThreashold": {
                    "type": "number"
                }
            },
            "required": [
                "compactionStrategy",
                "compactionThreashold"
            ]
        }
    }
}

)"_json;

auto main(int argc, char *argv[]) -> int
{
    // Construct options
    cxxopts::Options options("tinykvpp", "A tiny database, powering big ideas");
    options.add_options()("c,config", "Path to JSON configuration of database", cxxopts::value<std::string>());

    // Parse options
    auto parsedOptions = options.parse(argc, argv);
    if ((parsedOptions.count("help") != 0U) || (parsedOptions.count("config") == 0U))
    {
        spdlog::info("{}", options.help());
        return EXIT_SUCCESS;
    }

    // Read and parse config file into json
    const auto config = parsedOptions["config"].as<std::string>();
    std::ifstream configStream(config);
    json configJson = json::parse(configStream);

    // Set root schema and validate config file
    json_validator validator; // create validator
    try
    {
        validator.set_root_schema(database_config_schema);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Wrong database config validation schema. {}", e.what());
        return EXIT_FAILURE;
    }

    try
    {
        validator.validate(configJson);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Database config validation failed. {}", e.what());
        return EXIT_FAILURE;
    }

    // Create database config based on parsed config
    auto dbConfig = config::make_shared();
    dbConfig->DatabaseConfig.DatabasePath = configJson["database"]["path"].template get<std::string>();

    // Create/open the database
    auto db = db::db_t(dbConfig);
    if (!db.open())
    {
        spdlog::error("Unable to open the database");
    }

    auto key = tk_key_t{"hello"};
    auto value = tk_value_t{"world"};
    db.put(key, value);
    auto record = db.get(key);
    if (record.has_value())
    {
        spdlog::info("Record key={} and value={}", record->m_key.m_key, record->m_value.m_value);
    }
    else
    {
        spdlog::error("Unable to find record with key={}", record->m_key.m_key);
    }

    return EXIT_SUCCESS;
}
