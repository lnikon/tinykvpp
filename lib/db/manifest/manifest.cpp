#include "structures/lsmtree/segments/helpers.h"
#include <db/manifest/manifest.h>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace db::manifest
{

static constexpr const std::string_view current_filename("current");

// Manifest file will be periodically rotated to we need a unique filename every
// time
auto manifest_filename() -> std::string
{
    return fmt::format("manifest_{}", structures::lsmtree::segments::helpers::uuid());
}

auto latest_manifest_filename(const fs::path_t &databasePath) -> std::string
{
    const auto current_path{databasePath / current_filename};
    spdlog::info("[VAGAG]: databasePath=${} current_path=${}", databasePath.string(), current_path.string());
    if (!fs::stdfs::exists(current_path))
    {
        std::ofstream current(current_path);
        if (!current.is_open())
        {
            throw std::runtime_error("unable to create \"current\" " + current_path.string());
        }

        const auto new_filename{manifest_filename()};
        current << new_filename << '\n';
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

manifest_t::manifest_t(config::shared_ptr_t config)
    : m_config{config}
{
}

auto manifest_t::open() -> bool
{
    m_name = latest_manifest_filename(m_config->DatabaseConfig.DatabasePath);
    m_path = m_config->DatabaseConfig.DatabasePath / m_name;
    m_log.emplace(fs::append_only_file_t{m_path.c_str()});

    return true;
}

auto manifest_t::path() -> fs::path_t
{
    return m_path;
}

bool manifest_t::add(record_t info)
{
    auto infoToString = [](auto &&info) -> std::string
    {
        std::stringstream stringStream;
        info.write(stringStream);
        return stringStream.str();
    };

    if (!m_enabled)
    {
        spdlog::info("Manifest at {} is disabled - skipping record addition", m_path.c_str());
        if (spdlog::get_level() == spdlog::level::debug)
        {
            spdlog::debug("Skipped record details: {}", std::visit(infoToString, info));
        }
        return true;
    }

    m_records.emplace_back(info);
    const std::string &infoSerialized = std::visit(infoToString, info);
    ssize_t            res = m_log->append({infoSerialized.c_str(), infoSerialized.size()});
    return res >= 0;
}

// trim from start (in place)
inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
inline void rtrim(std::string &str)
{
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
              str.end());
}

inline auto trim(std::string &str) -> std::string &
{
    rtrim(str);
    ltrim(str);
    return str;
}

auto manifest_t::recover() -> bool
{
    spdlog::info("Manifest recovery started");

    std::int32_t record_type_int{0};
    std::string  line;

    auto stringStream = m_log->stream();
    while (std::getline(stringStream, line))
    {
        if (trim(line).empty())
        {
            continue;
        }

        std::istringstream lineStream(line);
        lineStream >> record_type_int;
        const auto record_type = static_cast<record_type_k>(record_type_int);
        switch (record_type)
        {
        case record_type_k::segment_k:
        {
            segment_record_t record;
            record.read(lineStream);
            spdlog::debug("recovered segment_record={}", record.ToString());
            m_records.emplace_back(record);
            break;
        }
        case record_type_k::level_k:
        {
            level_record_t record;
            record.read(lineStream);
            spdlog::debug("recovered level_record={}", record.ToString());
            m_records.emplace_back(record);
            break;
        }
        default:
        {
            spdlog::error("unhandled record_type_int={}. Skipping record.", record_type_int);
            break;
        }
        }
    }
    spdlog::info("Manifest recovery finished");

    return true;
}

auto manifest_t::path() const noexcept -> fs::path_t
{
    return m_path;
}

auto manifest_t::records() const noexcept -> std::vector<record_t>
{
    return m_records;
}

void manifest_t::enable()
{
    m_enabled = true;
    spdlog::info("Manifest at {} enabled - ready to record changes", m_path.c_str());
    spdlog::debug("Manifest enable triggered with {} pending records", m_records.size());
}

void manifest_t::disable()
{
    m_enabled = false;
    spdlog::info("Manifest at {} disabled - changes will not be recorded", m_path.c_str());
    spdlog::debug("Manifest disable triggered with {} pending records", m_records.size());
}
} // namespace db::manifest
