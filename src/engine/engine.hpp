#pragma once

#include <cstdint>
#include <optional>

#include "storage/memtable.hpp"

namespace frankie::engine {

class engine final {
 public:
  static constexpr std::uint64_t kDefaultMemtableCapacity = 64ULL * 1024 * 1024;  // 64MB

  [[nodiscard]] static engine create(std::uint64_t memtable_capacity = kDefaultMemtableCapacity) noexcept;

  engine() = default;
  engine(const engine &) = delete;
  engine &operator=(const engine &) = delete;
  engine(engine &&) = default;
  engine &operator=(engine &&) = default;
  ~engine() = default;

  void put(std::string_view key, std::string_view value) noexcept;

  [[nodiscard]] std::optional<std::string_view> get(std::string_view key) noexcept;

  void del(std::string_view key) noexcept;

  // TODO(lnikon): Interface tdb
  void scan(std::string_view range_start_key, std::string_view range_end_key) noexcept;

 private:
  void maybe_rotate_memtable(std::uint64_t incoming_bytes) noexcept;

  std::uint64_t memtable_capacity_{kDefaultMemtableCapacity};
  storage::memtable memtable_active_;
  std::optional<storage::memtable> memtable_immutable_;

  std::uint64_t sequence_{0};

  // TODO(lnikon): In a far, far future...
  // wal wal_;
};

}  // namespace frankie::engine
