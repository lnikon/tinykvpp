#include <absl/strings/ascii.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "db/manifest/manifest.h"

namespace db::manifest
{

manifest_t::manifest_t(fs::path_t path, wal::shared_ptr_t<manifest_t::record_t> wal) noexcept
    : m_path{std::move(path)},
      m_pWal{std::move(wal)}
{
}

auto manifest_t::path() const noexcept -> fs::path_t
{
    return m_path;
}

auto manifest_t::add(record_t info) -> bool
{
    return m_enabled ? m_pWal->add(std::move(info)) : [this]() -> bool
    {
        spdlog::info("Manifest at {} is disabled - skipping record addition", m_path.c_str());
        return false;
    }();
}

auto manifest_t::records() const noexcept -> std::vector<record_t>
{
    return m_pWal->records();
}

void manifest_t::enable()
{
    m_enabled = true;
    spdlog::info("Manifest at {} enabled - ready to record changes", m_path.c_str());
    spdlog::debug("Manifest enable triggered with {} pending records", m_pWal->size());
}

void manifest_t::disable()
{
    m_enabled = false;
    spdlog::info("Manifest at {} disabled - changes will not be recorded", m_path.c_str());
    spdlog::debug("Manifest disable triggered with {} pending records", m_pWal->size());
}

auto manifest_builder_t::build(fs::path_t path, wal::shared_ptr_t<manifest_t::record_t> pWal)
    -> std::optional<manifest_t>
{
    return std::make_optional(manifest_t{std::move(path), std::move(pWal)});
}

} // namespace db::manifest
