#include "structures/lsmtree/segments/helpers.h"
#include <db/manifest/manifest.h>
#include <fmt/format.h>
#include <stdexcept>

namespace db::manifest
{

// Manifest file will be periodically rotated to we need a unique filename every time
std::string manifest_filename()
{
    return fmt::format("manifest_{}", structures::lsmtree::segments::helpers::uuid());
}

manifest_t::manifest_t(const fs::path_t path)
    : m_path{path},
      m_log{path}
{
    // TODO(lnikon): How to handle this scenario without exceptions?
    if (!m_log.is_open())
    {
        throw std::runtime_error("unable to open manifest file: " + path.string());
    }
}

} // namespace db::manifest
