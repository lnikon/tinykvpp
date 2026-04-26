#pragma once

#include <cstdint>
#include <optional>

#include "core/config.hpp"
#include "core/status.hpp"
#include "engine/wal.hpp"
#include "storage/memtable.hpp"

namespace frankie::engine {

class engine final {
 public:
  engine() = default;
  engine(const engine &) = delete;
  engine &operator=(const engine &) = delete;
  engine(engine &&) noexcept = default;
  engine &operator=(engine &&) noexcept = default;
  ~engine() = default;

  [[nodiscard]] static std::expected<engine, core::status> create(const core::config &config) noexcept;

  [[nodiscard]] std::expected<void, core::status> put(std::string_view key, std::string_view value) noexcept;

  [[nodiscard]] std::expected<std::string_view, core::status> get(std::string_view key) noexcept;

  [[nodiscard]] std::expected<void, core::status> del(std::string_view key) noexcept;

  // TODO(lnikon): Interface tdb. Needs memtable iterator. Should search in both active and immutable?
  void scan(std::string_view range_start_key, std::string_view range_end_key) noexcept;

 private:
  [[nodiscard]] std::expected<void, core::status> maybe_rotate_memtable(std::uint64_t incoming_bytes) noexcept;
  [[nodiscard]] std::uint64_t get_next_sequence() noexcept;

  storage::memtable memtable_active_;
  std::optional<storage::memtable> memtable_immutable_;

  std::uint64_t sequence_{0};

  wal_writer wal_;
};

}  // namespace frankie::engine
