#include "fs/types.h"
#include "structures/lsmtree/segments/helpers.h"
#include <db/manifest/manifest.h>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <stdexcept>

namespace db::manifest
{

const std::string current_filename("current");

// Manifest file will be periodically rotated to we need a unique filename every time
std::string manifest_filename()
{
    return fmt::format("manifest_{}", structures::lsmtree::segments::helpers::uuid());
}

std::string latest_manifest_filename(const fs::path_t database_path)
{
    const auto current_path{database_path / current_filename};
    if (!fs::stdfs::exists(current_path))
    {
        std::fstream current(current_path, std::fstream::out | std::fstream::trunc);
        if (!current.is_open())
        {
            throw std::runtime_error("unable to open \"current\" " + current_path.string());
        }

        const auto new_filename{manifest_filename()};
        current << new_filename << std::endl;
        current.flush();
        return new_filename;
    }
    else
    {
        std::fstream current(current_path, std::fstream::in);
        if (!current.is_open())
        {
            throw std::runtime_error("unable to open \"current\" " + current_path.string());
        }

        std::string latest_filename;
        current >> latest_filename;
        return latest_filename;
    }
}

manifest_t::manifest_t(const config::shared_ptr_t config)
    : m_name{latest_manifest_filename(config->DatabaseConfig.DatabasePath)},
      m_path{config->DatabaseConfig.DatabasePath / m_name},
      m_log{m_path}
{
    // TODO(lnikon): How to handle this scenario without exceptions?
    if (!m_log.is_open())
    {
        throw std::runtime_error("unable to open manifest file: " + m_path.string());
    }
}

} // namespace db::manifest
