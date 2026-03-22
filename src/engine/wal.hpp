#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "core/arena.hpp"
#include "core/scratch_arena.hpp"
#include "engine/engine.hpp"

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
  std::uint64_t sequence;
  std::uint8_t tombstone;
  std::uint64_t key_size;
  std::uint64_t value_size;

  [[nodiscard]] static wal_entry create(core::arena &arena) noexcept;

  // TODO(lnikon): Return std::byte instead?
  [[nodiscard]] std::string_view encode(core::scratch_arena &arena) const noexcept;

  [[nodiscard]] static wal_entry decode(std::string_view encoded) noexcept;

  [[nodiscard]] std::string_view key() const noexcept;

  [[nodiscard]] std::string_view value() const noexcept;
};

// TODO(lnikon): Should the arena be injected from the engine?
class wal_writer final {
 public:
  [[nodiscard]] static wal_writer open(std::filesystem::path path, std::uint64_t capacity) noexcept;

  [[nodiscard]] bool append(wal_entry entry) noexcept;

  [[nodiscard]] bool sync() noexcept;

  void close() noexcept;

 private:
  std::filesystem::path path_;

  core::arena arena_;
  std::uint64_t capacity_;

  // To avoid temporary allocs on each encoding
  core::scratch_arena scratch_arena_;
};

void foo() {
  // TODO(lnikon): What if open fails? return std::expected?
  wal_writer wal = wal_writer::open("/path/to/wal", engine::kDefaultWalCapacity);

  wal_entry e;
  // TODO(lnikon): Need better error codes from core::error rather than bools
  // TODO(lnikon): Sync on each append?
  if (!wal.append(e)) {
    // TODO(lnikon): how to handle failed append?
  }

  if (!wal.sync()) {
    // TODO(lnikon): how to handle failed sync?
  }

  // TODO(lnikon): Can close fail?
  wal.close();
}

}  // namespace frankie::engine
