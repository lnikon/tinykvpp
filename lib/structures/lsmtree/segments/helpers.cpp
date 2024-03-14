//
// Created by nikon on 3/8/24.
//

#include <structures/lsmtree/segments/helpers.h>
#include <format>

namespace structures::lsmtree::segments::helpers
{

auto unix_timestamp()

{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

types::name_t segment_name()
{
    return types::name_t{std::format("segment_{}", unix_timestamp())};
}

std::filesystem::path segment_path(const std::filesystem::path datadir,
                                   const types::name_t& name)
{
    return datadir / name;
}

}  // namespace structures::lsmtree::segments::helpers
