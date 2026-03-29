#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

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
  std::uint8_t tombstone_;

  [[nodiscard]] std::string_view encode(core::scratch_arena &arena) const noexcept;

  [[nodiscard]] static std::optional<wal_entry> decode(std::string_view encoded) noexcept;
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

  [[nodiscard]] bool close() noexcept;

 private:
  std::filesystem::path path_;
  std::int32_t fd_{-1};

  std::uint64_t capacity_{0};

  core::scratch_arena scratch_arena_;
};

}  // namespace frankie::engine
