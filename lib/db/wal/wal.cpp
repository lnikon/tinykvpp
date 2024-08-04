#include <db/wal/wal.h>

#include <sstream>
#include <string>

namespace db::wal
{

auto wal_filename() -> std::string
{
    return {"wal"};
}

wal_t::wal_t(fs::path_t path)
    : m_path{std::move(path)}
{
}

auto wal_t::open() -> bool
{
    return m_log.open(m_path);
}

auto wal_t::path() -> fs::path_t
{
    return m_path;
}

void wal_t::add(record_t rec) noexcept
{
    m_records.push_back(rec);

    std::stringstream strStream;
    rec.write(strStream);
    m_log.write(strStream.str());

    spdlog::debug("Added new WAL entry {}", strStream.str());
}

void wal_t::reset()
{
    m_log.close();
    fs::stdfs::remove(m_path);
    if (!m_log.open())
    {
        throw std::runtime_error("unable to reset wal " + m_path.string());
    }

    spdlog::info("wal reset is successfull " + m_path.string());
}

auto wal_t::recover() noexcept -> bool
{
    auto recordToString = [](const record_t &rec) -> std::string
    {
        std::stringstream strStream;
        rec.write(strStream);
        return strStream.str();
    };

    spdlog::info("WAL recovery started");
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
        record_t rec;
        rec.read(lineStream);
        m_records.emplace_back(rec);

        spdlog::debug("Recovered WAL record {}", recordToString(rec));
    }
    spdlog::info("WAL recovery finished");

    return true;
}

auto wal_t::records() noexcept -> std::vector<record_t>
{
    return m_records;
}

} // namespace db::wal