#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "core/config.hpp"
#include "core/fs.hpp"
#include "core/scratch_arena.hpp"

namespace frankie::engine {

enum class wal_operation : std::uint8_t {
  put,
  del,
};

struct wal_entry final {
  static constexpr std::uint32_t kMetadataSize = 4     // record_len
                                                 + 4   // crc32
                                                 + 1   // operation
                                                 + 8   // sequence
                                                 + 1   // tombstone
                                                 + 4   // key_len
                                                 + 4;  // value_len

  wal_operation operation_;
  std::uint64_t sequence_;
  std::string_view key_;
  std::string_view value_;
  bool tombstone_;

  [[nodiscard]] std::string_view encode(core::scratch_arena &arena) const noexcept;

  [[nodiscard]] static std::optional<wal_entry> decode(std::string_view &encoded) noexcept;
};

class wal_writer final {
 public:
  wal_writer() = default;
  wal_writer(const wal_writer &) = delete;
  wal_writer &operator=(const wal_writer &) = delete;
  wal_writer(wal_writer &&) noexcept;
  wal_writer &operator=(wal_writer &&) noexcept;
  ~wal_writer() noexcept;

  [[nodiscard]] static std::optional<wal_writer> open(std::filesystem::path path, std::uint64_t capacity) noexcept;

  [[nodiscard]] bool append(const wal_entry &entry) noexcept;

  [[nodiscard]] bool sync() noexcept;

  [[nodiscard]] bool truncate() noexcept;

  [[nodiscard]] bool close() noexcept;

 private:
  core::append_only_file file_;

  std::uint64_t capacity_{core::kDefaultWalCapacity};

  core::scratch_arena scratch_arena_;
};

class wal_reader final {
 public:
  wal_reader() = default;
  wal_reader(const wal_reader &) = delete;
  wal_reader &operator=(const wal_reader &) = delete;
  wal_reader(wal_reader &&) noexcept;
  wal_reader &operator=(wal_reader &&) noexcept;
  ~wal_reader() noexcept;

  // Slurps the entire WAL into an arena buffer at open time. Returns nullopt
  // for missing or empty files (caller treats as "nothing to recover").
  [[nodiscard]] static std::optional<wal_reader> open(std::filesystem::path path) noexcept;

  [[nodiscard]] std::optional<wal_entry> read() noexcept;

  [[nodiscard]] bool close() noexcept;

 private:
  core::random_access_file file_;
  core::scratch_arena scratch_arena_;
  std::string_view wal_view_;
};

}  // namespace frankie::engine
