#include <config/config.h>

namespace config {

[[nodiscard]] std::filesystem::path config_t::get_segments_path() const {
  return DatabaseConfig.DatabasePath / LSMTreeConfig.SegmentsDirectoryName;
}

} // namespace config
