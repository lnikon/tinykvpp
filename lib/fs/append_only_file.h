#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>

#include <string>
#include <sstream>
#include <stdexcept>

namespace fs
{

static constexpr std::size_t gBufferSize{4096ULL};

class append_only_file_t
{
    int      fd_;
    io_uring ring_;

  public:
    append_only_file_t(const char *path, bool direct_io = false);

    ~append_only_file_t();

    ssize_t append(std::string_view data);

    ssize_t read(size_t offset, char *buffer, size_t size);

    std::size_t size() const;

    void flush();

    void reset();

    auto stream() -> std::stringstream;
};

} // namespace fs
