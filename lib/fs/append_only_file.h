#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>

#include <expected>

#include "common.h"

namespace fs
{

class append_only_file_t
{
  public:
    append_only_file_t() = delete;

    append_only_file_t(append_only_file_t &&other) noexcept;
    auto operator=(append_only_file_t &&other) noexcept -> append_only_file_t &;

    append_only_file_t(const append_only_file_t &) = delete;
    auto operator=(const append_only_file_t &) -> append_only_file_t & = delete;

    ~append_only_file_t() noexcept;

    [[nodiscard]] auto append(std::string_view data) noexcept -> std::expected<ssize_t, file_error_t>;
    [[nodiscard]] auto read(size_t offset, char *buffer, size_t size) noexcept -> std::expected<ssize_t, file_error_t>;
    [[nodiscard]] auto size() const noexcept -> std::expected<std::size_t, file_error_t>;
    [[nodiscard]] auto flush() noexcept -> std::expected<void, file_error_t>;
    [[nodiscard]] auto reset() noexcept -> std::expected<void, file_error_t>;
    [[nodiscard]] auto stream() noexcept -> std::expected<std::stringstream, file_error_t>;

    friend class append_only_file_builder_t;

  private:
    append_only_file_t(int fd, io_uring ring) noexcept;

    int      m_fd{-1};
    io_uring m_ring{};
};

class append_only_file_builder_t
{
  public:
    auto build(std::string path, bool direct_io) -> std::expected<append_only_file_t, file_error_t>;
};

} // namespace fs
