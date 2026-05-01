#pragma once

#include <fcntl.h>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string_view>
#include <type_traits>

#include "core/status.hpp"

// File primitives. Two unrelated types live here:
//   - random_access_file: positional read/write via pread/pwrite. SSTables, etc.
//   - append_only_file:   sequential append-only write via ::write + O_APPEND. WAL.
//
// They are deliberately distinct: pwrite + O_APPEND on Linux silently ignores the
// offset and writes at EOF (POSIX leaves this implementation-defined). Keeping
// the modes in separate types makes that combination unrepresentable.
namespace frankie::core {

constexpr std::int32_t kDefaultFilePermissions = 0644;

enum class access_mode : std::int8_t { read_only = O_RDONLY, write_only = O_WRONLY, read_write = O_RDWR };

enum class open_flag : std::int32_t {
  none = 0,
  append = O_APPEND,
  creat = O_CREAT,
  truncate = O_TRUNC,
  exclusive = O_EXCL,
  nonblock = O_NONBLOCK,
  sync = O_SYNC,
  direct = O_DIRECT,
};

[[nodiscard]] constexpr open_flag operator|(open_flag lhs, open_flag rhs) noexcept {
  using U = std::underlying_type_t<open_flag>;
  return static_cast<open_flag>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

[[nodiscard]] constexpr open_flag operator&(open_flag lhs, open_flag rhs) noexcept {
  using U = std::underlying_type_t<open_flag>;
  return static_cast<open_flag>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

[[nodiscard]] constexpr open_flag &operator|=(open_flag &lhs, open_flag rhs) noexcept {
  lhs = lhs | rhs;
  return lhs;
}

[[nodiscard]] constexpr bool any(open_flag flag) noexcept {
  using U = std::underlying_type_t<open_flag>;
  return static_cast<U>(flag) != 0;
}

template <typename TEnum>
[[nodiscard]] constexpr auto to_native(TEnum flags) noexcept {
  return static_cast<std::underlying_type_t<TEnum>>(flags);
}

// Stateless positional file. pread/pwrite only. No O_APPEND factory: that
// combination is a footgun (pwrite ignores the offset under O_APPEND on Linux).
// Use append_only_file for append workloads.
class random_access_file final {
 public:
  random_access_file() = default;
  random_access_file(const random_access_file &) = delete;
  random_access_file &operator=(const random_access_file &) = delete;
  random_access_file(random_access_file &&) noexcept;
  random_access_file &operator=(random_access_file &&) noexcept;
  ~random_access_file() noexcept;

  [[nodiscard]] static std::expected<random_access_file, core::status> open_read(std::filesystem::path path) noexcept;

  [[nodiscard]] static std::expected<random_access_file, core::status> create_exclusive(
      std::filesystem::path path) noexcept;

  [[nodiscard]] static std::expected<random_access_file, core::status> create_or_truncate(
      std::filesystem::path path) noexcept;

  // Writes `data` at `offset`. Loops over EINTR and short writes. Does not fsync.
  [[nodiscard]] std::expected<std::uint64_t, core::status> write(std::string_view data, std::uint64_t offset) noexcept;

  // Writes `data` at `offset` and fdatasyncs. Loops over EINTR and short writes.
  [[nodiscard]] std::expected<std::uint64_t, core::status> write_fsync(std::string_view data,
                                                                       std::uint64_t offset) noexcept;

  // Single pread at `offset`. Loops over EINTR. Returns eof on zero bytes read.
  // Caller handles short reads.
  [[nodiscard]] std::expected<std::uint64_t, core::status> read(std::span<char> data, std::uint64_t offset) noexcept;

  [[nodiscard]] std::expected<void, core::status> sync() noexcept;

  [[nodiscard]] std::expected<std::uint64_t, core::status> size() noexcept;

  [[nodiscard]] std::expected<void, core::status> truncate() noexcept;

  [[nodiscard]] std::expected<void, core::status> close() noexcept;

  [[nodiscard]] std::filesystem::path path() const noexcept;

 private:
  std::filesystem::path path_;
  std::int32_t fd_{-1};
};

// Sequential append-only file. Opened with O_WRONLY | O_APPEND | O_CREAT.
// No offsets in the public API: every append goes to current EOF atomically
// (relative to other writers on the same inode). Uses ::write, not pwrite.
class append_only_file final {
 public:
  append_only_file() = default;
  append_only_file(const append_only_file &) = delete;
  append_only_file &operator=(const append_only_file &) = delete;
  append_only_file(append_only_file &&) noexcept;
  append_only_file &operator=(append_only_file &&) noexcept;
  ~append_only_file() noexcept;

  // Creates the file if missing, opens append-only otherwise.
  [[nodiscard]] static std::expected<append_only_file, core::status> open(std::filesystem::path path) noexcept;

  // Appends `data` to EOF. Loops over EINTR and short writes. Does not fsync.
  [[nodiscard]] std::expected<std::uint64_t, core::status> append(std::string_view data) noexcept;

  // Appends and fdatasyncs.
  [[nodiscard]] std::expected<std::uint64_t, core::status> append_fsync(std::string_view data) noexcept;

  [[nodiscard]] std::expected<void, core::status> sync() noexcept;

  [[nodiscard]] std::expected<std::uint64_t, core::status> size() noexcept;

  // Truncates to zero bytes. Next append starts at offset 0.
  [[nodiscard]] std::expected<void, core::status> truncate() noexcept;

  [[nodiscard]] std::expected<void, core::status> close() noexcept;

  [[nodiscard]] std::filesystem::path path() const noexcept;

 private:
  std::filesystem::path path_;
  std::int32_t fd_{-1};
};

}  // namespace frankie::core
