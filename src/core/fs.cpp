#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <expected>
#include <limits>
#include <print>
#include <utility>

#include "core/assert.hpp"
#include "core/fs.hpp"
#include "core/status.hpp"

namespace frankie::core {

namespace {

// open(2) + parent-dir fsync when O_CREAT is set. Returns the raw fd or io_error.
// Shared between random_access_file and append_only_file.
std::expected<std::int32_t, core::status> open_fd(const std::filesystem::path &path, access_mode mode,
                                                  open_flag flags) noexcept {
  const std::int32_t fd = ::open(path.c_str(), to_native(mode) | to_native(flags), kDefaultFilePermissions);
  if (fd == -1) {
    std::println("fs::open_fd: failed to open file. path={}, errno={} ({})", path.c_str(), errno, strerror(errno));
    return core::unexpected(status_code::io_error);
  }

  if (any(flags & open_flag::creat)) {
    auto parent_dir_path = path.parent_path();
    if (parent_dir_path.empty()) {
      parent_dir_path = ".";
    }

    const std::int32_t parent_dir_fd = ::open(parent_dir_path.c_str(), O_RDONLY | O_DIRECTORY);
    if (parent_dir_fd == -1) {
      ::close(fd);
      std::println("fs::open_fd: failed to open parent dir. dir={}, errno={} ({})", parent_dir_path.c_str(), errno,
                   strerror(errno));
      return core::unexpected(status_code::io_error);
    }
    const std::int32_t parent_rc = ::fsync(parent_dir_fd);
    ::close(parent_dir_fd);
    if (parent_rc != 0) {
      ::close(fd);
      std::println("fs::open_fd: parent dir fsync failed. dir={}, errno={} ({})", parent_dir_path.c_str(), errno,
                   strerror(errno));
      return core::unexpected(status_code::io_error);
    }
  }

  return fd;
}

// pwrite full buffer at `offset`. Retries EINTR. Loops on short writes.
std::expected<std::uint64_t, core::status> pwrite_all(std::int32_t fd, std::string_view data,
                                                      std::uint64_t offset) noexcept {
  std::uint64_t total = 0;
  while (total < data.size()) {
    const ssize_t n = ::pwrite(fd, data.data() + total, data.size() - total, static_cast<off_t>(offset + total));
    if (n == -1) {
      if (errno == EINTR) {
        continue;
      }
      std::println("fs::pwrite_all: pwrite failed. fd={}, errno={} ({})", fd, errno, strerror(errno));
      return core::unexpected(status_code::io_error);
    }
    if (n == 0) {
      // pwrite returning 0 on a regular file is not expected; treat as error.
      std::println("fs::pwrite_all: pwrite returned 0. fd={}, total={}, requested={}", fd, total, data.size());
      return core::unexpected(status_code::io_error);
    }
    total += static_cast<std::uint64_t>(n);
  }
  return total;
}

// ::write full buffer to current fd offset (i.e. EOF under O_APPEND).
// Retries EINTR. Loops on short writes.
std::expected<std::uint64_t, core::status> write_all(std::int32_t fd, std::string_view data) noexcept {
  std::uint64_t total = 0;
  while (total < data.size()) {
    const ssize_t n = ::write(fd, data.data() + total, data.size() - total);
    if (n == -1) {
      if (errno == EINTR) {
        continue;
      }
      std::println("fs::write_all: write failed. fd={}, errno={} ({})", fd, errno, strerror(errno));
      return core::unexpected(status_code::io_error);
    }
    if (n == 0) {
      std::println("fs::write_all: write returned 0. fd={}, total={}, requested={}", fd, total, data.size());
      return core::unexpected(status_code::io_error);
    }
    total += static_cast<std::uint64_t>(n);
  }
  return total;
}

std::expected<void, core::status> fdatasync_fd(std::int32_t fd) noexcept {
  if (::fdatasync(fd) != 0) {
    std::println("fs: fdatasync failed. fd={}, errno={} ({})", fd, errno, strerror(errno));
    return core::unexpected(status_code::io_error);
  }
  return {};
}

std::expected<std::uint64_t, core::status> fstat_size(std::int32_t fd, const std::filesystem::path &path) noexcept {
  struct stat statbuf{};
  if (::fstat(fd, &statbuf) == -1) {
    std::println("fs::fstat_size: fstat failed. fd={}, path={}, errno={} ({})", fd, path.c_str(), errno,
                 strerror(errno));
    return core::unexpected(status_code::io_error);
  }
  if (statbuf.st_size < 0) {
    std::println("fs::fstat_size: negative file size. fd={}, path={}", fd, path.c_str());
    return core::unexpected(status_code::invalid_argument);
  }
  return static_cast<std::uint64_t>(statbuf.st_size);
}

std::expected<void, core::status> ftruncate_zero(std::int32_t fd, const std::filesystem::path &path) noexcept {
  if (::ftruncate(fd, 0) == -1) {
    std::println("fs::ftruncate_zero: ftruncate failed. fd={}, path={}, errno={} ({})", fd, path.c_str(), errno,
                 strerror(errno));
    return core::unexpected(status_code::io_error);
  }
  return {};
}

std::expected<void, core::status> close_fd(std::int32_t &fd) noexcept {
  if (fd == -1) {
    return {};
  }
  // ::close releases the fd even on failure; no retry possible.
  const std::int32_t rc = ::close(fd);
  fd = -1;
  if (rc != 0) {
    std::println("fs::close_fd: close failed. errno={} ({})", errno, strerror(errno));
    return core::unexpected(status_code::io_error);
  }
  return {};
}

}  // namespace

// =============================================================================
// random_access_file
// =============================================================================

random_access_file::random_access_file(random_access_file &&other) noexcept
    : path_{std::exchange(other.path_, {})}, fd_{std::exchange(other.fd_, -1)} {}

random_access_file &random_access_file::operator=(random_access_file &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  (void)close();

  path_ = std::exchange(other.path_, {});
  fd_ = std::exchange(other.fd_, -1);

  return *this;
}

random_access_file::~random_access_file() noexcept {
  if (fd_ != -1) {
    (void)close();
  }
}

std::expected<random_access_file, core::status> random_access_file::open_read(std::filesystem::path path) noexcept {
  auto fd = open_fd(path, access_mode::read_only, open_flag::none);
  if (!fd) {
    return core::unexpected(fd.error().code_);
  }
  random_access_file result;
  result.path_ = std::move(path);
  result.fd_ = fd.value();
  return result;
}

std::expected<random_access_file, core::status> random_access_file::create_exclusive(
    std::filesystem::path path) noexcept {
  auto fd = open_fd(path, access_mode::read_write, open_flag::creat | open_flag::exclusive);
  if (!fd) {
    return core::unexpected(fd.error().code_);
  }
  random_access_file result;
  result.path_ = std::move(path);
  result.fd_ = fd.value();
  return result;
}

std::expected<random_access_file, core::status> random_access_file::create_or_truncate(
    std::filesystem::path path) noexcept {
  auto fd = open_fd(path, access_mode::read_write, open_flag::creat | open_flag::truncate);
  if (!fd) {
    return core::unexpected(fd.error().code_);
  }
  random_access_file result;
  result.path_ = std::move(path);
  result.fd_ = fd.value();
  return result;
}

std::expected<std::uint64_t, core::status> random_access_file::write(std::string_view data,
                                                                     std::uint64_t offset) noexcept {
  static_assert(sizeof(off_t) >= sizeof(std::uint64_t), "build with _FILE_OFFSET_BITS=64");
  FR_VERIFY_MSG(fd_ != -1, "file descriptor should not be empty");
  FR_VERIFY_MSG(offset <= static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()), "offset overflow");

  return pwrite_all(fd_, data, offset);
}

std::expected<std::uint64_t, core::status> random_access_file::write_fsync(std::string_view data,
                                                                           std::uint64_t offset) noexcept {
  auto written = write(data, offset);
  if (!written) {
    return written;
  }
  if (auto s = fdatasync_fd(fd_); !s) {
    return core::unexpected(s.error().code_);
  }
  return written;
}

std::expected<std::uint64_t, core::status> random_access_file::read(std::span<char> data,
                                                                    std::uint64_t offset) noexcept {
  static_assert(sizeof(off_t) >= sizeof(std::uint64_t), "build with _FILE_OFFSET_BITS=64");
  FR_VERIFY_MSG(fd_ != -1, "file descriptor should not be empty");
  FR_VERIFY_MSG(offset <= static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()), "offset overflow");
  FR_VERIFY(!data.empty());

  ssize_t bytes_read = 0;
  for (;;) {
    bytes_read = ::pread(fd_, data.data(), data.size(), static_cast<off_t>(offset));
    if (bytes_read != -1) {
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    std::println("random_access_file::read: pread failed. fd={}, path={}, requested={}, offset={}, errno={} ({})", fd_,
                 path_.c_str(), data.size(), offset, errno, strerror(errno));
    return core::unexpected(status_code::io_error);
  }

  if (bytes_read == 0) {
    return core::unexpected(status_code::eof);
  }

  return static_cast<std::uint64_t>(bytes_read);
}

std::expected<void, core::status> random_access_file::sync() noexcept {
  FR_VERIFY_MSG(fd_ != -1, "file descriptor should not be empty");
  return fdatasync_fd(fd_);
}

std::expected<std::uint64_t, core::status> random_access_file::size() noexcept {
  FR_VERIFY_MSG(fd_ != -1, "file descriptor should not be empty");
  return fstat_size(fd_, path_);
}

std::expected<void, core::status> random_access_file::truncate() noexcept {
  FR_VERIFY_MSG(fd_ != -1, "file descriptor should not be empty");
  return ftruncate_zero(fd_, path_);
}

std::expected<void, core::status> random_access_file::close() noexcept { return close_fd(fd_); }

std::filesystem::path random_access_file::path() const noexcept { return path_; }

// =============================================================================
// append_only_file
// =============================================================================

append_only_file::append_only_file(append_only_file &&other) noexcept
    : path_{std::exchange(other.path_, {})}, fd_{std::exchange(other.fd_, -1)} {}

append_only_file &append_only_file::operator=(append_only_file &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  (void)close();

  path_ = std::exchange(other.path_, {});
  fd_ = std::exchange(other.fd_, -1);

  return *this;
}

append_only_file::~append_only_file() noexcept {
  if (fd_ != -1) {
    (void)close();
  }
}

std::expected<append_only_file, core::status> append_only_file::open(std::filesystem::path path) noexcept {
  auto fd = open_fd(path, access_mode::write_only, open_flag::creat | open_flag::append);
  if (!fd) {
    return core::unexpected(fd.error().code_);
  }
  append_only_file result;
  result.path_ = std::move(path);
  result.fd_ = fd.value();
  return result;
}

std::expected<std::uint64_t, core::status> append_only_file::append(std::string_view data) noexcept {
  FR_VERIFY_MSG(fd_ != -1, "file descriptor should not be empty");
  return write_all(fd_, data);
}

std::expected<std::uint64_t, core::status> append_only_file::append_fsync(std::string_view data) noexcept {
  auto written = append(data);
  if (!written) {
    return written;
  }
  if (auto s = fdatasync_fd(fd_); !s) {
    return core::unexpected(s.error().code_);
  }
  return written;
}

std::expected<void, core::status> append_only_file::sync() noexcept {
  FR_VERIFY_MSG(fd_ != -1, "file descriptor should not be empty");
  return fdatasync_fd(fd_);
}

std::expected<std::uint64_t, core::status> append_only_file::size() noexcept {
  FR_VERIFY_MSG(fd_ != -1, "file descriptor should not be empty");
  return fstat_size(fd_, path_);
}

std::expected<void, core::status> append_only_file::truncate() noexcept {
  FR_VERIFY_MSG(fd_ != -1, "file descriptor should not be empty");
  return ftruncate_zero(fd_, path_);
}

std::expected<void, core::status> append_only_file::close() noexcept { return close_fd(fd_); }

std::filesystem::path append_only_file::path() const noexcept { return path_; }

}  // namespace frankie::core
