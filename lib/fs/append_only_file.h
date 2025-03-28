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
  public:
    append_only_file_t(const char *path, bool direct_io = false);

    append_only_file_t(append_only_file_t &&other) noexcept;
    auto operator=(append_only_file_t &&other) noexcept -> append_only_file_t &;

    append_only_file_t(const append_only_file_t &) = delete;
    auto operator=(const append_only_file_t &) -> append_only_file_t & = delete;

    ~append_only_file_t() noexcept;

    [[nodiscard]] auto append(std::string_view data) -> ssize_t;
    [[nodiscard]] auto read(size_t offset, char *buffer, size_t size) -> ssize_t;
    [[nodiscard]] auto size() const -> std::size_t;
    void               flush();
    void               reset();
    [[nodiscard]] auto stream() -> std::stringstream;

  private:
    int      fd_{-1};
    io_uring ring_{};
};

} // namespace fs
