#include "random_access_file.h"

#include "fs/common.h"

#include <liburing.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <format>

namespace fs::random_access_file
{

random_access_file_t::random_access_file_t(int fd, io_uring ring) noexcept
    : m_fd(fd),
      m_ring(ring)
{
}

random_access_file_t::random_access_file_t(random_access_file_t &&other) noexcept
{
    if (this == &other)
    {
        return;
    }

    m_fd = other.m_fd;
    m_ring = other.m_ring;

    other.m_fd = -1;
    other.m_ring = io_uring{};
}

auto random_access_file_t::operator=(random_access_file_t &&other) noexcept
    -> random_access_file_t &
{
    if (this == &other)
    {
        return *this;
    }

    m_fd = other.m_fd;
    m_ring = other.m_ring;

    other.m_fd = -1;
    other.m_ring = io_uring{};

    return *this;
}

random_access_file_t::~random_access_file_t() noexcept
{
    // Skip destructor call on moved-from objects
    // TODO: Need a reference counting for this class to close the last instance
    if (m_fd == -1)
    {
        return;
    }

    io_uring_queue_exit(&m_ring);
    close(m_fd);
}

auto random_access_file_t::write(std::string_view data, ssize_t offset) noexcept
    -> std::expected<ssize_t, file_error_t>
{
    io_uring_sqe *sqe = io_uring_get_sqe(&m_ring);
    if (sqe == nullptr)
    {
        return std::unexpected(file_error_t{
            .code = file_error_code_k::write_failed,
            .system_errno = 0,
            .message = std::format("Failed to get io_uring sqe. fd={}", m_fd),
        });
    }

    auto iov = iovec{.iov_base = const_cast<char *>(data.data()), .iov_len = data.size()};
    io_uring_prep_writev(sqe, m_fd, &iov, 1, offset);
    sqe->flags |= IOSQE_IO_LINK;

    io_uring_submit(&m_ring);
    io_uring_cqe *cqe;
    io_uring_wait_cqe(&m_ring, &cqe);
    auto res = cqe->res;
    io_uring_cqe_seen(&m_ring, cqe);

    if (res < 0)
    {
        return std::unexpected(file_error_t{
            .code = file_error_code_k::write_failed,
            .system_errno = -res,
            .message = std::format("Write operation failed. fd={}", m_fd),
        });
    }
    return res;
}

auto random_access_file_t::read(ssize_t offset, char *buffer, size_t size) noexcept
    -> std::expected<ssize_t, file_error_t>
{
    // return pread(m_fd, buffer, size, offset);
    ssize_t res = pread(m_fd, buffer, size, offset);
    if (res < 0)
    {
        return std::unexpected(file_error_t{
            .code = file_error_code_k::read_failed,
            .system_errno = errno,
            .message = std::format("Read operation failed. fd={}", m_fd),
        });
    }
    return res;
}

auto random_access_file_t::size() const noexcept -> std::expected<std::size_t, file_error_t>
{
    struct stat st;
    if (fstat(m_fd, &st) == -1)
    {
        return std::unexpected(file_error_t{
            .code = file_error_code_k::stat_failed,
            .system_errno = errno,
            .message = std::format("Failed to get file size. fd={}", m_fd),
        });
    }
    return static_cast<std::size_t>(st.st_size);
}

auto random_access_file_t::flush() noexcept -> std::expected<void, file_error_t>
{
    if (fsync(m_fd) != 0)
    {
        return std::unexpected(file_error_t{
            .code = file_error_code_k::flush_failed,
            .system_errno = errno,
            .message = std::format("Flush operation failed. fd={}", m_fd),
        });
    }
    return {};
}

auto random_access_file_t::reset() noexcept -> std::expected<void, file_error_t>
{
    if (ftruncate(m_fd, 0) != 0)
    {
        return std::unexpected(file_error_t::from_errno(
            file_error_code_k::truncate_failed, errno, "File truncate operation failed"));
    }

    if (lseek(m_fd, 0, SEEK_SET) < 0)
    {
        return std::unexpected(file_error_t::from_errno(
            file_error_code_k::seek_failed, errno, "File seek operation failed"));
    }

    return {};
}

auto random_access_file_t::stream() noexcept -> std::expected<std::stringstream, file_error_t>
{
    auto size_result = size();
    if (!size_result)
    {
        return std::unexpected(size_result.error());
    }

    if (*size_result == 0)
    {
        return std::stringstream{};
    }

    std::string       buffer(kBufferSize, '\0');
    std::size_t       offset{0};
    std::stringstream result;

    while (true)
    {
        auto read_result = read(offset, buffer.data(), kBufferSize);
        if (!read_result)
        {
            return std::unexpected(read_result.error());
        }

        auto bytes_read = *read_result;
        if (bytes_read == 0)
        {
            break; // End of file
        }

        result.write(buffer.data(), bytes_read);
        offset += bytes_read;
    }

    return result;
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
 * @return On success, returns an random_access_file_t object. On failure,
 * returns a file_error with details about what went wrong:
 *         - file_error_code_k::open_failed if path is empty or open() fails
 *         - file_error_code_k::io_uring_init_failed if io_uring initialization
 * fails
 *
 * @note Uses kDefaultFilePermissions for file creation and kIOUringQueueEntries
 *       for the io_uring queue depth.
 */
auto random_access_file_builder_t::build(fs::path_t path, posix_wrapper::open_flag_k openFlags)
    -> std::expected<random_access_file_t, file_error_t>
{
    if (path.empty())
    {
        return std::unexpected(file_error_t{
            .code = file_error_code_k::open_failed,
            .system_errno = errno,
            .message = std::format("Provided file path is empty"),
        });
    }

    int fdes = open(path.c_str(), posix_wrapper::to_native(openFlags), kDefaultFilePermissions);
    if (fdes < 0)
    {
        return std::unexpected(file_error_t{
            .code = file_error_code_k::open_failed,
            .system_errno = errno,
            .message = std::format("Failed to open file. path={}", path.c_str()),
        });
    }

    io_uring ring{};
    if (int res = io_uring_queue_init(kIOUringQueueEntries, &ring, 0); res < 0)
    {
        close(fdes);
        return std::unexpected(file_error_t{
            .code = file_error_code_k::io_uring_init_failed,
            .system_errno = errno,
            .message = std::format("io_uring_queue_init failed"),
        });
    }

    spdlog::debug("Opened file. path={}, fd={}", path.c_str(), fdes);

    return random_access_file_t{fdes, ring};
}

} // namespace fs::random_access_file
