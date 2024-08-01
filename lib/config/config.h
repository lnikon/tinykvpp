#pragma once

#include <db/db_config.h>
#include <structures/lsmtree/lsmtree_config.h>
#include <structures/lsmtree/segments/segment_config.h>

namespace config
{

struct config_t
{
    db::db_config_t DatabaseConfig;
    structures::lsmtree::lsmtree_config_t LSMTreeConfig;
    structures::lsmtree::segments::segment_config_t SegmentConfig;

    [[nodiscard]] auto datadir_path() const -> std::filesystem::path;
};

using shared_ptr_t = std::shared_ptr<config_t>;

template <typename... Args>
auto make_shared(Args... args)
{
    return std::make_shared<config_t>(std::forward(args)...);
}

}  // namespace config
