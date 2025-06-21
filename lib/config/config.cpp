#include <filesystem>
#include <fstream>

#include <fmt/format.h>

#include "config.h"
#include "structures/lsmtree/segments/helpers.h"

namespace config
{

static constexpr const std::string_view SegmentsDirectoryName{"segments"};
static constexpr const std::string_view ManifestCurrentFilename("current");

[[nodiscard]] auto config_t::datadir_path() const -> fs::path_t
{
    return DatabaseConfig.DatabasePath / SegmentsDirectoryName;
}

[[nodiscard]] auto config_t::manifest_path() const -> fs::path_t
{
    const auto current_path{DatabaseConfig.DatabasePath / ManifestCurrentFilename};
    if (!fs::stdfs::exists(current_path))
    {
        std::ofstream current(current_path);
        if (!current.is_open())
        {
            throw std::runtime_error("unable to create \"current\" " + current_path.string());
        }

        const auto new_filename{
            DatabaseConfig.DatabasePath /
            fmt::format("manifest_{}", structures::lsmtree::segments::helpers::uuid())
        };
        current << new_filename.c_str() << '\n';
        current.flush();

        return new_filename;
    }

    std::ifstream current(current_path);
    if (!current.is_open())
    {
        throw std::runtime_error("unable to open \"current\" " + current_path.string());
    }

    std::string latest_filename;
    current >> latest_filename;
    return latest_filename;
}

} // namespace config
