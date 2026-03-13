#pragma once

#include <format>
#include <optional>

#include "core/scratch_arena.h"
#include "storage/skiplist.hpp"

namespace frankie::storage {

struct internal_key_comparator {
  constexpr int operator()(const std::string_view a, const std::string_view b) const noexcept {
    return simd_comparator{}(a.substr(0, a.size() - 17), b.substr(0, b.size() - 17));
  }
};
static_assert(Comparator<internal_key_comparator>);

struct kv_entry final {
  std::string_view key_;
  std::string_view value_;
  std::uint64_t sequence_;
  std::uint64_t timestamp_;
  bool tombstone_;

  [[nodiscard]] std::string_view user_key() const noexcept;

  [[nodiscard]] std::string_view internal_key(core::scratch_arena &arena) const noexcept;

  [[nodiscard]] std::string_view value() const noexcept;

  [[nodiscard]] std::uint64_t bytes_allocated() const noexcept;
};

class memtable final {
 public:
  [[nodiscard]] static memtable create(std::uint64_t capacity) noexcept;

  void put(std::string_view key, std::string_view value, std::uint64_t sequence, bool is_tombstone) noexcept;

  [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const noexcept;

  [[nodiscard]] std::uint64_t count() const noexcept;

 private:
  mutable core::scratch_arena scratch_arena_{};
  core::arena arena_{};
  skiplist<internal_key_comparator> skiplist_;

  std::uint64_t count_{0};
  std::uint64_t capacity_{0};

  std::uint64_t min_sequence_{std::numeric_limits<std::uint64_t>::max()};
  std::uint64_t max_sequence_{std::numeric_limits<std::uint64_t>::min()};
};

}  // namespace frankie::storage
