#include <db/manifest/manifest.h>
#include <stdexcept>

namespace db::manifest
{

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
