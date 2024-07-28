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

std::string uuid();

[[nodiscard]] auto unix_timestamp();

[[nodiscard]] types::name_t segment_name();

[[nodiscard]] std::filesystem::path segment_path(const std::filesystem::path datadir, const types::name_t &name);

} // namespace structures::lsmtree::segments::helpers

#endif // ZKV_HELPERS_H
