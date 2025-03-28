#include "append_only_file.h"

#include <cstring>
#include <iostream>

fs::append_only_file_t::append_only_file_t(const char *path, bool direct_io)
    : fd_(open(path, O_RDWR | O_CREAT | O_APPEND | (direct_io ? O_DIRECT : 0), 0644))
{
    // TODO(lnikon): Maybe better to use separate is_open() interface?
    if (fd_ < 0)
    {
        throw std::runtime_error(std::format("Failed to open file path={} errno={}", path, strerror(errno)));
    }

    std::cout << "[VAGAG]: opened file: " << path << "; fd=" << fd_ << "\n" << std::endl;

    // io_uring initialization
    io_uring_queue_init(128, &ring_, 0);
}

fs::append_only_file_t::append_only_file_t(append_only_file_t &&other) noexcept
{
    if (this == &other)
    {
        return;
    }

    fd_ = other.fd_;
    ring_ = other.ring_;

    other.fd_ = -1;
    other.ring_ = io_uring{};
}

auto fs::append_only_file_t::operator=(append_only_file_t &&other) noexcept -> append_only_file_t &
{
    if (this == &other)
    {
        return *this;
    }

    fd_ = other.fd_;
    ring_ = other.ring_;

    other.fd_ = -1;
    other.ring_ = io_uring{};

    return *this;
}

fs::append_only_file_t::~append_only_file_t() noexcept
{
    // Skip destructor call on moved-from objects
    if (fd_ == -1)
    {
        return;
    }

    io_uring_queue_exit(&ring_);
    close(fd_);
}

ssize_t fs::append_only_file_t::append(std::string_view data)
{
    io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
    auto          iov = iovec{.iov_base = const_cast<char *>(data.data()), .iov_len = data.size()};
    io_uring_prep_writev(sqe, fd_, &iov, 1, 0);
    sqe->flags |= IOSQE_IO_LINK;

    io_uring_submit(&ring_);
    io_uring_cqe *cqe;
    io_uring_wait_cqe(&ring_, &cqe);
    auto res = cqe->res;

    io_uring_cqe_seen(&ring_, cqe);
    return res;
}

ssize_t fs::append_only_file_t::read(size_t offset, char *buffer, size_t size)
{
    return pread(fd_, buffer, size, offset);
}

void fs::append_only_file_t::flush()
{

    fsync(fd_);
}

void fs::append_only_file_t::reset()
{
    ftruncate(fd_, 0);
    lseek(fd_, 0, SEEK_SET);
}

auto fs::append_only_file_t::stream() -> std::stringstream
{
    if (size() == 0)
    {
        return {};
    }

    std::string       buffer(gBufferSize, '\0');
    std::size_t       offset{0};
    std::stringstream result;

    while (true)
    {
        ssize_t res = read(offset, buffer.data(), gBufferSize);
        if (res == 0)
        {
            break;
        }

        if (res == -1)
        {
            throw std::runtime_error(std::format("Failed to read from file. fd={} errno={}", fd_, strerror(errno)));
        }

        result.write(buffer.data(), res);
        offset += res;
    }

    return result;
}

auto fs::append_only_file_t::size() const -> std::size_t
{
    struct stat st;
    if (fstat(fd_, &st) == -1)
    {
        throw std::runtime_error(std::format("Failed to get file size. fd={} errno={}", fd_, strerror(errno)));
    }
    return st.st_size;
}
