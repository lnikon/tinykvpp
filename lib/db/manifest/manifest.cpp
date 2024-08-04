#include "fs/types.h"
#include "structures/lsmtree/segments/helpers.h"
#include <db/manifest/manifest.h>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace db::manifest
{

const std::string current_filename("current");

// Manifest file will be periodically rotated to we need a unique filename every time
auto manifest_filename() -> std::string
{
    return fmt::format("manifest_{}", structures::lsmtree::segments::helpers::uuid());
}

auto latest_manifest_filename(const fs::path_t databasePath) -> std::string
{
    const auto current_path{databasePath / current_filename};
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
    return m_log.open(m_path);
}

auto manifest_t::path() -> fs::path_t
{
    return m_path;
}

void manifest_t::add(record_t info)
{
    if (!m_enabled)
    {
        spdlog::info("Manifest at {} is disabled", m_path.c_str());
        return;
    }

    m_records.emplace_back(info);

    std::stringstream stringStream;
    std::visit([&stringStream](auto &&record) { record.write(stringStream); }, info);
    m_log.write(stringStream.str());
}

void manifest_t::print() const
{
    for (const auto &rec : m_records)
    {
        std::visit(
            [](auto &&arg)
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, segment_record_t>)
                {
                    spdlog::info("{} on with name {} level {}", arg.ToString(arg.op), arg.name, arg.level);
                }
                else if constexpr (std::is_same_v<T, level_record_t>)
                {
                    spdlog::info("{} on level {}", arg.ToString(arg.op), arg.level);
                }
            },
            rec);
    }
}

auto manifest_t::recover() -> bool
{
    spdlog::info("Manifest recovery started");
    auto stringStream = m_log.stream();
    std::int32_t record_type_int{0};
    std::string line;
    while (std::getline(stringStream, line))
    {
        if (line.empty())
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
            record.read(stringStream);
            spdlog::info("recovered segment_record={}", record.ToString());
            m_records.emplace_back(record);
            break;
        }
        case record_type_k::level_k:
        {
            level_record_t record;
            record.read(stringStream);
            spdlog::info("recovered level_record={}", record.ToString());
            m_records.emplace_back(record);
            break;
        }
        default:
        {
            spdlog::error("undhandled record_type_int={}. Skipping record.", record_type_int);
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
    spdlog::info("Manifest at {} enabled", m_path.c_str());
}

void manifest_t::disable()
{
    m_enabled = false;
    spdlog::info("Manifest at {} disabled", m_path.c_str());
}
} // namespace db::manifest
