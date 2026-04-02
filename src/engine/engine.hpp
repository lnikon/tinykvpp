#pragma once

#include <cstdint>
#include <expected>
#include <optional>

#include "engine/wal.hpp"
#include "storage/memtable.hpp"

namespace frankie::engine {

class engine final {
 public:
  static constexpr std::uint64_t kDefaultMemtableCapacity = 64ULL * 1024 * 1024;  // 64MB
  static constexpr std::uint64_t kDefaultWalCapacity =
      kDefaultMemtableCapacity;  // Seems reasonable to fallback to memtable's capacity

  engine() = default;
  engine(const engine &) = delete;
  engine &operator=(const engine &) = delete;
  engine(engine &&) noexcept = default;
  engine &operator=(engine &&) noexcept = default;
  ~engine() = default;

  [[nodiscard]] static std::optional<engine> create(std::filesystem::path wal_path,
                                                    std::uint64_t memtable_capacity = kDefaultMemtableCapacity,
                                                    std::uint64_t wal_capacity = kDefaultWalCapacity) noexcept;

  [[nodiscard]] bool put(std::string_view key, std::string_view value) noexcept;

  [[nodiscard]] std::optional<std::string_view> get(std::string_view key) noexcept;

  [[nodiscard]] bool del(std::string_view key) noexcept;

  // TODO(lnikon): Interface tdb
  void scan(std::string_view range_start_key, std::string_view range_end_key) noexcept;

 private:
  void maybe_rotate_memtable(std::uint64_t incoming_bytes) noexcept;
  [[nodiscard]] std::uint64_t get_next_sequence() noexcept;

  std::uint64_t memtable_capacity_{kDefaultMemtableCapacity};
  storage::memtable memtable_active_;
  std::optional<storage::memtable> memtable_immutable_;

  std::uint64_t sequence_{0};

  wal_writer wal_;
};

}  // namespace frankie::engine
