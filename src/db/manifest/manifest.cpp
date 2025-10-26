#include <absl/strings/ascii.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "db/manifest/manifest.h"

namespace db::manifest
{

manifest_t::manifest_t(fs::path_t path, wal::wal_t<manifest_t::record_t> wal) noexcept
    : m_path{std::move(path)},
      m_wal{std::move(wal)}
{
}

auto manifest_t::path() const noexcept -> fs::path_t
{
    return m_path;
}

auto manifest_t::add(record_t info) -> bool
{
    return m_enabled ? m_wal.add(std::move(info)) : [this]()
    {
        spdlog::info("Manifest at {} is disabled - skipping record addition", m_path.c_str());
        return false;
    }();
}

auto manifest_t::records() const noexcept -> std::vector<record_t>
{
    return m_wal.records();
}

void manifest_t::enable()
{
    m_enabled = true;
    spdlog::info("Manifest at {} enabled - ready to record changes", m_path.c_str());
    spdlog::debug("Manifest enable triggered with {} pending records", m_wal.size());
}

void manifest_t::disable()
{
    m_enabled = false;
    spdlog::info("Manifest at {} disabled - changes will not be recorded", m_path.c_str());
    spdlog::debug("Manifest disable triggered with {} pending records", m_wal.size());
}

auto manifest_builder_t::build(fs::path_t path, wal::wal_t<manifest_t::record_t> wal)
    -> std::optional<manifest_t>
{
    return std::make_optional(manifest_t{std::move(path), std::move(wal)});
}

} // namespace db::manifest
