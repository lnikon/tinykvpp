#include "wal.h"

namespace wal
{

wal_t::wal_t(log_t log) noexcept
    : m_log{std::move(log)}
{
}

wal_t::wal_t(wal_t &&other) noexcept
    : m_log(std::move(other.m_log))
{
}

auto wal_t::operator=(wal_t &&other) noexcept -> wal_t &
{
    if (this == &other)
    {
        return *this;
    }
    m_log = std::move(other.m_log);
    return *this;
}

auto wal_t::add(const record_t &rec) noexcept -> bool
{
    const auto op_view{magic_enum::enum_name(rec.op)};
    if (!m_log.append(std::string{op_view.data(), op_view.size()},
                      rec.kv.m_key.m_key,
                      rec.kv.m_value.m_value))
    {
        spdlog::error(std::format("WAL: Failed to append entry: {} {} {}",
                                  op_view,
                                  rec.kv.m_key.m_key,
                                  rec.kv.m_value.m_value));
        return false;
    }

    spdlog::debug(
        "WAL: Appended new entry: {} {} {}", op_view, rec.kv.m_key.m_key, rec.kv.m_value.m_value);

    return true;
}

auto wal_t::reset() noexcept -> bool
{
    return m_log.reset();
}

[[nodiscard]] auto wal_t::records() const -> std::vector<record_t>
{
    auto recordToString = [](const record_t &rec)
    {
        std::stringstream strStream;
        rec.write(strStream);
        return strStream.str();
    };

    auto logStream = std::stringstream{};
    for (std::size_t idx{0}; idx < m_log.size(); ++idx)
    {
        if (auto logLine{m_log.read(idx)}; logLine.has_value())
        {
            logStream << logLine.value();
        }
    }

    std::string           line;
    std::vector<record_t> result;
    while (std::getline(logStream, line))
    {
        if (absl::StripAsciiWhitespace(line).empty())
        {
            spdlog::debug("wal::records() empty line after trim. skipping");
            continue;
        }

        std::istringstream lineStream(line);
        record_t           rec;
        rec.read(lineStream);

        spdlog::debug("WAL: Recovered record: {}", recordToString(rec));

        result.emplace_back(std::move(rec));
    }
    spdlog::info("WAL: Recovery finished");

    return result;
}

} // namespace wal
