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

    std::ifstream current(current_path);
    if (!current.is_open())
    {
        // File doesn't exist, create it
        std::ofstream current_out(current_path);
        if (!current_out.is_open())
        {
            throw std::runtime_error("unable to create \"current\" " + current_path.string());
        }

        const auto new_filename{
            DatabaseConfig.DatabasePath /
            fmt::format("manifest_{}", structures::lsmtree::segments::helpers::uuid())
        };
        current_out << new_filename.string() << '\n';
        current_out.flush();
        current_out.close();

        return new_filename;
    }

    std::string latest_filename;
    if (!(current >> latest_filename) || latest_filename.empty())
    {
        throw std::runtime_error("invalid or empty manifest filename in \"current\" file");
    }

    return latest_filename;
}

} // namespace config
