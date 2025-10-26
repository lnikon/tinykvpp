#include <cstdio>
#include <cstring>
#include <fcntl.h>

#include <liburing.h>
#include <spdlog/spdlog.h>

#include "fs/append_only_file.h"
#include "fs/common.h"
#include "posix_wrapper/open_flag.h"

namespace pw = posix_wrapper;

namespace fs
{

fs::append_only_file_t::append_only_file_t(random_access_file_t &&fd) noexcept
    : m_fd(std::move(fd))
{
}

fs::append_only_file_t::append_only_file_t(append_only_file_t &&other) noexcept
    : m_fd{std::move(other.m_fd)}
{
    if (this == &other)
    {
        return;
    }
}

auto fs::append_only_file_t::operator=(append_only_file_t &&other) noexcept -> append_only_file_t &
{
    if (this == &other)
    {
        return *this;
    }

    m_fd = std::move(other.m_fd);
    return *this;
}

fs::append_only_file_t::~append_only_file_t() noexcept
{
}

auto fs::append_only_file_t::append(std::string_view data) noexcept
    -> std::expected<ssize_t, file_error_t>
{
    // Accordinig to https://man7.org/linux/man-pages/man3/io_uring_prep_writev.3.html
    // "On files that support seeking, if the offset is set to -1, the write operation commences at
    // the file offset, and the file offset is incremented by the number of bytes written."
    return m_fd.write(data, -1);
}

auto fs::append_only_file_t::read(ssize_t offset, char *buffer, size_t size) noexcept
    -> std::expected<ssize_t, file_error_t>
{
    return m_fd.read(offset, buffer, size);
}

auto fs::append_only_file_t::size() const noexcept -> std::expected<std::size_t, file_error_t>
{
    return m_fd.size();
}

auto fs::append_only_file_t::flush() noexcept -> std::expected<void, file_error_t>
{
    return m_fd.flush();
}

auto fs::append_only_file_t::reset() noexcept -> std::expected<void, file_error_t>
{
    return m_fd.reset();
}

auto fs::append_only_file_t::stream() noexcept -> std::expected<std::stringstream, file_error_t>
{
    return m_fd.stream();
}

/**
 * @brief Creates an append-only file with the specified path.
 *
 * This function creates or opens a file in append-only mode. The file can be
 * opened with direct I/O if specified. The function initializes an io_uring for
 * asynchronous I/O operations on the file.
 *
 * @param path The path to the file to be opened or created.
 * @param direct_io If true, the file is opened with O_DIRECT flag for direct
 * I/O.
 *
 * @return On success, returns an append_only_file_t object. On failure, returns
 *         a file_error with details about what went wrong:
 *         - file_error_code_k::open_failed if path is empty or open() fails
 *         - file_error_code_k::io_uring_init_failed if io_uring initialization
 * fails
 *
 * @note Uses kDefaultFilePermissions for file creation and kIOUringQueueEntries
 *       for the io_uring queue depth.
 */
auto append_only_file_builder_t::build(std::string path, bool direct_io)
    -> std::expected<append_only_file_t, file_error_t>
{
    auto flags{pw::open_flag_k::kReadWrite | pw::open_flag_k::kCreate | pw::open_flag_k::kAppend};
    if (direct_io)
    {
        flags = flags | pw::open_flag_k::kDirect;
    }

    auto file{random_access_file_builder_t{}.build(std::move(path), flags)};
    if (!file.has_value())
    {
        return std::unexpected(file.error());
    }

    return append_only_file_t{std::move(file.value())};
}

} // namespace fs
