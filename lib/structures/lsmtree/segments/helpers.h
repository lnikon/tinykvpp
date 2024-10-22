//
// Created by nikon on 3/8/24.
//

#ifndef ZKV_HELPERS_H
#define ZKV_HELPERS_H

#include <config/config.h>
#include <structures/lsmtree/segments/types.h>

#include <filesystem>

namespace structures::lsmtree::segments::helpers
{

auto uuid() -> std::string;

[[nodiscard]] auto unix_timestamp();

[[nodiscard]] auto segment_name() -> types::name_t;

[[nodiscard]] auto segment_path(const std::filesystem::path &datadir,
                                const types::name_t         &name) -> std::filesystem::path;

} // namespace structures::lsmtree::segments::helpers

#endif // ZKV_HELPERS_H
