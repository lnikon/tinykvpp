#pragma once

#include <expected>

#include <liburing.h>

#include "common.h"
#include "fs/types.h"
#include "posix_wrapper/open_flag.h"

namespace fs::random_access_file
{

class random_access_file_t
{
public:
  random_access_file_t() = delete;

  random_access_file_t(random_access_file_t &&other) noexcept;
  auto operator=(random_access_file_t &&other) noexcept
      -> random_access_file_t &;

  random_access_file_t(const random_access_file_t &) = delete;
  auto operator=(const random_access_file_t &)
      -> random_access_file_t & = delete;

  ~random_access_file_t() noexcept;

  [[nodiscard]] auto write(std::string_view data, size_t offset) noexcept
      -> std::expected<ssize_t, file_error_t>;

  [[nodiscard]] auto read(size_t offset, char *buffer, size_t size) noexcept
      -> std::expected<ssize_t, file_error_t>;

  [[nodiscard]] auto size() const noexcept
      -> std::expected<std::size_t, file_error_t>;

  [[nodiscard]] auto flush() noexcept -> std::expected<void, file_error_t>;

  [[nodiscard]] auto reset() noexcept -> std::expected<void, file_error_t>;

  [[nodiscard]] auto stream() noexcept
      -> std::expected<std::stringstream, file_error_t>;

  friend class random_access_file_builder_t;

private:
  random_access_file_t(int fd, io_uring ring) noexcept;

  int      m_fd{-1};
  io_uring m_ring{};
};

class random_access_file_builder_t
{
public:
  auto build(fs::path_t path, posix_wrapper::open_flag_k openFlags)
      -> std::expected<random_access_file_t, file_error_t>;
};

} // namespace fs::random_access_file
