#pragma once

#include <nlohmann/json.hpp>

#include "db/db_config.h"
#include "structures/lsmtree/lsmtree_config.h"
#include "structures/lsmtree/segments/segment_config.h"
#include "server/server_config.h"
#include "wal/config.h"

namespace config
{

struct config_t
{
    db::db_config_t                                 DatabaseConfig;
    structures::lsmtree::lsmtree_config_t           LSMTreeConfig;
    structures::lsmtree::segments::segment_config_t SegmentConfig;
    server::server_config_t                         ServerConfig;
    wal::config_t                                   WALConfig;

    [[nodiscard]] auto datadir_path() const -> fs::path_t;
    [[nodiscard]] auto manifest_path() const -> fs::path_t;
};

using shared_ptr_t = std::shared_ptr<config_t>;

template <typename... Args> auto make_shared(Args &&...args)
{
    return std::make_shared<config_t>(std::forward(args)...);
}

auto loadConfigJson(const std::string &configPath) -> nlohmann::json;

void validateConfigJson(const nlohmann::json &configJson);

void configureLogging(const std::string &loggingLevel);

auto loadDatabaseConfig(const nlohmann::json &configJson) -> config::shared_ptr_t;

void loadWALConfig(const nlohmann::json &walConfig, config::shared_ptr_t dbConfig);

void loadLSMTreeConfig(
    const nlohmann::json &lsmtreeConfig,
    config::shared_ptr_t  dbConfig,
    const std::string    &configPath
);

auto loadServerConfig(const nlohmann::json &configJson, config::shared_ptr_t dbConfig);

auto initializeDatabaseConfig(const nlohmann::json &configJson, const std::string &configPath)
    -> config::shared_ptr_t;
} // namespace config
