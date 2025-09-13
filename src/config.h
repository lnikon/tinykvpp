#pragma once

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>

#include "config/config.h"

using nlohmann::json;
using nlohmann::json_schema::json_validator;
using json = nlohmann::json;

auto loadConfigJson(const std::string &configPath) -> json;

void validateConfigJson(const json &configJson);

void configureLogging(const std::string &loggingLevel);

auto loadDatabaseConfig(const json &configJson) -> config::shared_ptr_t;

void loadWALConfig(const json &walConfig, config::shared_ptr_t dbConfig);

void loadLSMTreeConfig(
    const json &lsmtreeConfig, config::shared_ptr_t dbConfig, const std::string &configPath
);

auto loadServerConfig(const json &configJson, config::shared_ptr_t dbConfig);

auto initializeDatabaseConfig(const json &configJson, const std::string &configPath)
    -> config::shared_ptr_t;
