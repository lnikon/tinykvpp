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

    explicit append_only_file_t(const path_t path);
    ~append_only_file_t() noexcept;

    bool open() noexcept;
    bool is_open() noexcept;

    /**
     * @brief Flush buffered data into the disk and close the underlying stream
     *
     * @return Result of closing the stream
     */
    bool close() noexcept;

    /**
     * @brief
     *
     * @param data
     * @return
     */
    bool write(const data_t &data) noexcept;

    std::stringstream stream() const noexcept;

  private:
    path_t m_path;
    std::fstream m_out;
};

inline append_only_file_t::append_only_file_t(const path_t path)
    : m_path{path},
      m_out{m_path, m_out.app | m_out.ate | m_out.out}
{
}

inline append_only_file_t::~append_only_file_t() noexcept
{
    close();
}

inline bool append_only_file_t::open() noexcept
{
    m_out = std::fstream{m_path, m_out.app | m_out.ate | m_out.out};
    return m_out.is_open();
}

inline bool append_only_file_t::is_open() noexcept
{
    return m_out.is_open();
}

inline bool append_only_file_t::close() noexcept
{
    if (!is_open())
    {
        return true;
    }

    m_out.flush();
    m_out.close();

    // TODO(lnikon): Do we need to recover when we're unable to close the stream?
    return m_out.bad();
}

inline bool append_only_file_t::write(const data_t &data) noexcept
{
    if (!is_open())
    {
        return false;
    }

    m_out << data << std::endl;
    m_out.flush();

    return m_out.bad();
}

inline std::stringstream append_only_file_t::stream() const noexcept
{
    std::stringstream ss;
    std::fstream fs(m_path);
    if (fs.is_open())
    {
        ss << fs.rdbuf();
    }
    return ss;
}

} // namespace fs
