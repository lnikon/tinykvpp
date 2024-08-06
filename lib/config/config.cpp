#include <config/config.h>

namespace config
{

/**
 * @brief Name of directory inside the database root dir where segments should be stored
 */
constexpr const std::string_view SegmentsDirectoryName{"segments"};

[[nodiscard]] auto config_t::datadir_path() const -> std::filesystem::path
{
    return DatabaseConfig.DatabasePath / SegmentsDirectoryName;
}

} // namespace config
