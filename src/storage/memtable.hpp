#pragma once

#include <optional>
#include <format>

#include "storage/skiplist.hpp"

namespace frankie::storage {

// ksz      key   vsz      value seq     tmstmp  [byte]
// [8-bytes][data][8-bytes][data][8-byte][8-byte][tomb]
struct kv_entry final {
  std::string_view key_;
  std::string_view value_;
  std::uint64_t sequence_;
  std::uint64_t timestamp_;
  bool tombstone_;

  [[nodiscard]] std::string_view user_key() const noexcept {
    return key_;
  }

  // key   seq     tmstmp  [tomb]
  // [data][8-byte][8-byte][byte]
  [[nodiscard]] std::string internal_key() const noexcept {
    return std::format("{}{}{}", key_, sequence_, timestamp_);
  }

  [[nodiscard]] std::string_view value() const noexcept {
    return value_;
  }
};

class memtable final {
 public:
  void put(std::string_view key, std::string_view value, std::uint64_t sequence, bool is_tombstone) noexcept;

  [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const noexcept;

  [[nodiscard]] std::uint64_t count() const noexcept;

 private:
  core::arena arena_{};
  skiplist<> skiplist_;

  std::uint64_t count_{0};
  std::uint64_t max_entries_;

  std::uint64_t min_sequence_;
  std::uint64_t max_sequence_;
};

}  // namespace frankie::storage
