#pragma once

#include <cassert>
#include <optional>

#include "core/assert.hpp"
#include "core/scratch_arena.hpp"
#include "storage/skiplist.hpp"

namespace frankie::storage {

// ================================================================================
// internal_key — user key + metadata, symmetric encode/decode
// ================================================================================
struct internal_key final {
  static constexpr std::uint64_t kMetadataSize = 8 + 8 + 1;  // sequence + timestamp + tombstone

  std::string_view user_key;
  std::uint64_t sequence;
  std::uint64_t timestamp;
  bool tombstone;

  [[nodiscard]] std::string_view encode(core::scratch_arena &arena) const noexcept;

  [[nodiscard]] static internal_key decode(std::string_view encoded) noexcept;
};

// ================================================================================
// internal_key_comparator - comparator aware of internal_key structure
// ================================================================================
struct internal_key_comparator {
  constexpr int operator()(const std::string_view a, const std::string_view b) const noexcept {
    FR_VERIFY(a.size() >= internal_key::kMetadataSize);
    FR_VERIFY(b.size() >= internal_key::kMetadataSize);
    return simd_comparator{}(a.substr(0, a.size() - internal_key::kMetadataSize),
                             b.substr(0, b.size() - internal_key::kMetadataSize));
  }
};
static_assert(Comparator<internal_key_comparator>);

// ================================================================================
// kv_entry — memtable entry & convenient wrappers
// ================================================================================
struct kv_entry final {
  std::string_view key_;
  std::string_view value_;
  std::uint64_t sequence_;
  std::uint64_t timestamp_;
  bool tombstone_;

  [[nodiscard]] std::string_view user_key() const noexcept;

  [[nodiscard]] std::string_view value() const noexcept;

  [[nodiscard]] std::uint64_t bytes_allocated() const noexcept;
};

// ================================================================================
// memtable — backed by frankie::core::arena & frankie::core::skiplist
// ================================================================================
class memtable final {
 public:
  memtable() = default;
  memtable(const memtable &) = delete;
  memtable &operator=(const memtable &) = delete;
  memtable(memtable &&) noexcept;
  memtable &operator=(memtable &&) noexcept;
  ~memtable();

  [[nodiscard]] static memtable create(std::uint64_t capacity) noexcept;

  void put(std::string_view key, std::string_view value, std::uint64_t sequence, bool is_tombstone) noexcept;

  [[nodiscard]] std::optional<kv_entry> get(std::string_view key) const noexcept;

  [[nodiscard]] std::uint64_t count() const noexcept;

  [[nodiscard]] std::uint64_t bytes_allocated() const noexcept;

  [[nodiscard]] std::uint64_t capacity() const noexcept;

 private:
  mutable core::scratch_arena scratch_arena_;
  core::arena arena_;
  skiplist<internal_key_comparator> skiplist_;

  std::uint64_t count_{0};
  std::uint64_t capacity_{0};
  std::uint64_t bytes_allocated_{0};

  std::uint64_t min_sequence_{std::numeric_limits<std::uint64_t>::max()};
  std::uint64_t max_sequence_{std::numeric_limits<std::uint64_t>::min()};
};

}  // namespace frankie::storage
