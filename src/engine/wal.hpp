#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>

#include "core/arena.hpp"
#include "core/scratch_arena.hpp"

namespace frankie::engine {

enum class wal_operation : std::uint8_t {
  put,
  del,
};

// [record_len: u64][crc32: u64][operation: u8][sequence: u64][tombstone: u8][key_size: u64][value_size:
// u64][key: u8[]][value: u8[]]
struct wal_entry final {
  static constexpr const std::uint64_t kMetadataSize = 8 + 8 + 1 + 8 + 1 + 8 + 8;

  wal_operation operation_;
  std::uint64_t sequence_;
  std::uint8_t tombstone_;
  std::uint64_t key_size_;
  std::uint64_t value_size_;

  [[nodiscard]] static wal_entry *create(core::arena &arena, wal_operation operation, std::uint64_t sequence,
                                         std::uint8_t tombstone, std::string_view key, std::string_view value) noexcept;

  // TODO(lnikon): Return std::byte instead?
  [[nodiscard]] std::string_view encode(core::scratch_arena &arena) const noexcept;

  [[nodiscard]] static wal_entry decode(std::string_view encoded) noexcept;

  [[nodiscard]] std::string_view key() const noexcept;

  [[nodiscard]] std::string_view value() const noexcept;

 private:
  [[nodiscard]] std::span<std::byte> key_bytes() noexcept;

  [[nodiscard]] std::span<const std::byte> key_bytes() const noexcept;

  [[nodiscard]] std::span<std::byte> value_bytes() noexcept;

  [[nodiscard]] std::span<const std::byte> value_bytes() const noexcept;
};

// TODO(lnikon): Should the arena be injected from the engine?
class wal_writer final {
 public:
  wal_writer() = default;
  wal_writer(const wal_writer &) = delete;
  wal_writer &operator=(const wal_writer &) = delete;
  wal_writer(wal_writer &&) noexcept = default;
  wal_writer &operator=(wal_writer &&) noexcept = default;
  ~wal_writer() = default;

  [[nodiscard]] static std::optional<wal_writer> open(std::filesystem::path path, std::uint64_t capacity) noexcept;

  [[nodiscard]] bool append(wal_operation operation, std::uint64_t sequence, std::uint8_t tombstone,
                            std::string_view key, std::string_view value) noexcept;

  [[nodiscard]] bool sync() noexcept;

  void close() noexcept;

 private:
  std::filesystem::path path_;
  std::fstream file_;

  core::arena arena_;
  std::uint64_t capacity_{0};

  core::scratch_arena scratch_arena_;
};

}  // namespace frankie::engine
