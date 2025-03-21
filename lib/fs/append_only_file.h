#pragma once

#include "types.h"

#include <string>
#include <sstream>
#include <fstream>

namespace fs
{

struct append_only_file_t
{
    using data_t = std::string;

    explicit append_only_file_t(fs::path_t path);
    append_only_file_t() = default;
    ~append_only_file_t() noexcept;

    auto open() noexcept -> bool;
    auto open(fs::path_t path) noexcept -> bool;
    auto is_open() noexcept -> bool;

    /**
     * @brief Flush buffered data into the disk and close the underlying stream
     *
     * @return Result of closing the stream
     */
    auto close() noexcept -> bool;

    /**
     * @brief
     *
     * @param data
     * @return
     */
    auto write(const data_t &data) noexcept -> bool;

    auto stream() const noexcept -> std::stringstream;

  private:
    path_t       m_path;
    std::fstream m_out;
};

inline append_only_file_t::append_only_file_t(fs::path_t path)
    : m_path{std::move(path)}
{
}

inline append_only_file_t::~append_only_file_t() noexcept
{
    close();
}

inline auto append_only_file_t::open() noexcept -> bool
{
    m_out = std::fstream{m_path, std::fstream::app | std::fstream::ate | std::fstream::out};
    return m_out.is_open();
}

inline auto append_only_file_t::open(fs::path_t path) noexcept -> bool
{
    m_path = std::move(path);
    return open();
}

inline auto append_only_file_t::is_open() noexcept -> bool
{
    return m_out.is_open();
}

inline auto append_only_file_t::close() noexcept -> bool
{
    if (!is_open())
    {
        return true;
    }

    m_out.flush();
    m_out.close();

    // TODO(lnikon): Do we need to recover when we're unable to close the
    // stream?
    return m_out.bad();
}

inline auto append_only_file_t::write(const data_t &data) noexcept -> bool
{
    if (!is_open())
    {
        return false;
    }

    m_out << data << '\n';
    m_out.flush();

    return m_out.bad();
}

inline auto append_only_file_t::stream() const noexcept -> std::stringstream
{
    std::stringstream fileStringStream;
    std::fstream      fileStream(m_path);
    if (fileStream.is_open())
    {
        fileStringStream << fileStream.rdbuf();
    }
    return fileStringStream;
}

} // namespace fs
