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
                "memtableFlushThreshold",
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

auto main(int argc, char *argv[]) -> int
{
    // Configure spdlog
    spdlog::set_level(spdlog::level::debug);

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

    // Read and parse configPath file into json
    const auto configPath = parsedOptions["config"].as<std::string>();
    std::fstream configStream(configPath, std::fstream::in);
    json configJson = json::parse(configStream);

    // Set root schema and validate configPath file
    json_validator validator; // create validator
    try
    {
        validator.set_root_schema(database_config_schema);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Wrong database configPath validation schema={}", e.what());
        return EXIT_FAILURE;
    }

    try
    {
        validator.validate(configJson);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Database configPath validation failed. Error={}", e.what());
        return EXIT_FAILURE;
    }

    // Create database configPath based on parsed configPath
    auto dbConfig = config::make_shared();

    // Fill database related fields
    if (configJson["database"].contains("path"))
    {
        dbConfig->DatabaseConfig.DatabasePath = configJson["database"]["path"].template get<std::string>();
    }

    if (configJson["database"].contains("walFilename"))
    {
        dbConfig->DatabaseConfig.WalFilename = configJson["database"]["walFilename"].template get<std::string>();
    }

    if (configJson["database"].contains("manifestFilenamePrefix"))
    {
        dbConfig->DatabaseConfig.ManifestFilenamePrefix =
            configJson["database"]["manifestFilenamePrefix"].template get<std::string>();
    }

    // Fill LSMTree related fields
    if (configJson.contains("lsmtree"))
    {
        const auto &lsmtreeConfig = configJson["lsmtree"];
        if (lsmtreeConfig.contains("memtableFlushThreshold"))
        {
            dbConfig->LSMTreeConfig.DiskFlushThresholdSize =
                lsmtreeConfig["memtableFlushThreshold"].template get<uint64_t>();
        }
        else
        {
            spdlog::error("\"memtableFlushThreshold\" is not specified in configPath {}", configPath);
            return EXIT_FAILURE;
        }

        if (lsmtreeConfig.contains("levelZeroCompaction"))
        {
            if (lsmtreeConfig["levelZeroCompaction"].contains("compactionStrategy"))
            {
                dbConfig->LSMTreeConfig.LevelZeroCompactionStrategy =
                    lsmtreeConfig["levelZeroCompaction"]["compactionStrategy"].template get<std::string>();
            }
            else
            {
                spdlog::error("\"levelZeroCompaction.compactionStrategy\" is not specified in configPath {}",
                              configPath);
                return EXIT_FAILURE;
            }

            if (lsmtreeConfig["levelZeroCompaction"].contains("compactionThreshold"))
            {
                dbConfig->LSMTreeConfig.LevelZeroCompactionThreshold =
                    lsmtreeConfig["levelZeroCompaction"]["compactionThreshold"].template get<std::uint64_t>();
            }
            else
            {
                spdlog::error("\"levelZeroCompaction.compactionThreshold\" is not specified in configPath {}",
                              configPath);
                return EXIT_FAILURE;
            }
        }
        else
        {
            spdlog::error("\"levelZeroCompaction\" is not specified in configPath {}", configPath);
            return EXIT_FAILURE;
        }

        if (lsmtreeConfig.contains("levelNonZeroCompaction"))
        {
            if (lsmtreeConfig["levelNonZeroCompaction"].contains("compactionStrategy"))
            {
                dbConfig->LSMTreeConfig.LevelNonZeroCompactionStrategy =
                    lsmtreeConfig["levelNonZeroCompaction"]["compactionStrategy"].template get<std::string>();
            }
            else
            {
                spdlog::error("\"levelNonZeroCompaction.compactionStrategy\" is not specified in configPath {}",
                              configPath);
                return EXIT_FAILURE;
            }

            if (lsmtreeConfig["levelNonZeroCompaction"].contains("compactionThreshold"))
            {
                dbConfig->LSMTreeConfig.LevelNonZeroCompactionThreshold =
                    lsmtreeConfig["levelNonZeroCompaction"]["compactionThreshold"].template get<std::uint64_t>();
            }
            else
            {
                spdlog::error("\"levelNonZeroCompaction.compactionThreshold\" is not specified in configPath {}",
                              configPath);
                return EXIT_FAILURE;
            }
        }
        else
        {
            spdlog::error("\"levelNonZeroCompaction\" is not specified in configPath {}", configPath);
            return EXIT_FAILURE;
        }
    }
    else
    {
        spdlog::error("\"lsmtree\" is not specified in configPath {}", configPath);
        return EXIT_FAILURE;
    }

    // Create/open the database
    auto db = db::db_t(dbConfig);
    if (!db.open())
    {
        spdlog::error("Unable to open the database");
    }

    auto key = tk_key_t{"goodbye"};
    auto value = tk_value_t{"internet"};
    db.put(key, value);

    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version1"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version2"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version3"});
    db.put(tk_key_t{"cccccc"}, tk_value_t{"aaaa"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version4"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version5"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version6"});
    db.put(tk_key_t{"cccccc"}, tk_value_t{"bbbb"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version7"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version8"});
    db.put(tk_key_t{"ddddd"}, tk_value_t{"version1"});
    db.put(tk_key_t{"cccccc"}, tk_value_t{"dddd"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version9"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version10"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version11"});
    db.put(tk_key_t{"cccccc1"}, tk_value_t{"aaaa1"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version12"});
    db.put(tk_key_t{"aaaaaa2"}, tk_value_t{"version13"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version13"});
    db.put(tk_key_t{"ddddd"}, tk_value_t{"version1"});
    db.put(tk_key_t{"cccccc"}, tk_value_t{"dddd"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version9"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version10"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version11"});
    db.put(tk_key_t{"cccccc1"}, tk_value_t{"aaaa1"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version12"});
    db.put(tk_key_t{"aaaaaa2"}, tk_value_t{"version13"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version13"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version1"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version2"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version3"});
    db.put(tk_key_t{"cccccc"}, tk_value_t{"aaaa"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version4"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version5"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version6"});
    db.put(tk_key_t{"cccccc"}, tk_value_t{"bbbb"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version7"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version8"});
    db.put(tk_key_t{"ddddd"}, tk_value_t{"version1"});
    db.put(tk_key_t{"cccccc"}, tk_value_t{"dddd"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version9"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version10"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version11"});
    db.put(tk_key_t{"cccccc1"}, tk_value_t{"aaaa1"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version12"});
    db.put(tk_key_t{"aaaaaa2"}, tk_value_t{"version13"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version13"});
    db.put(tk_key_t{"ddddd"}, tk_value_t{"version1"});
    db.put(tk_key_t{"cccccc"}, tk_value_t{"dddd"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version9"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version10"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version11"});
    db.put(tk_key_t{"cccccc1"}, tk_value_t{"aaaa1"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version12"});
    db.put(tk_key_t{"aaaaaa2"}, tk_value_t{"version13"});
    db.put(tk_key_t{"aaaaaa"}, tk_value_t{"version13"});

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
