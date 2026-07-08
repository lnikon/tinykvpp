#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/config.hpp"
#include "core/scratch_arena.hpp"
#include "core/status.hpp"
#include "engine/wal.hpp"
#include "storage/memtable.hpp"
#include "storage/segment.hpp"

namespace frankie::engine {

inline constexpr std::uint32_t kEngineFlushArenaCapacity = 4096;
inline constexpr std::uint32_t kEngineScratchArenaCapacity = 4096;

class engine final {
  [[nodiscard]] std::expected<void, core::status> maybe_rotate_memtable() noexcept;
  [[nodiscard]] std::uint64_t get_next_sequence() noexcept;

  core::config config_;
  // Scratch arena shared between all engine flows. Use only for temporary work.
  core::scratch_arena scratch_arena_;

  storage::memtable memtable_active_;
  std::optional<storage::memtable> memtable_immutable_;

  std::uint64_t sequence_{0};

  wal_writer wal_;

  // Semgent management. Temporary.
  std::vector<storage::segment> segments_;
  std::string_view sstable_file_prefix_{"sst_"};
  std::uint64_t sstable_id_{0};

 public:
  engine() = default;
  engine(const engine &) = delete;
  engine &operator=(const engine &) = delete;
  engine(engine &&) noexcept = default;
  engine &operator=(engine &&) noexcept = default;
  ~engine() = default;

  [[nodiscard]] static std::expected<engine, core::status> create(core::config config) noexcept;

  [[nodiscard]] std::expected<void, core::status> put(std::string_view key, std::string_view value) noexcept;

  [[nodiscard]] std::expected<std::string_view, core::status> get(std::string_view key) noexcept;

  [[nodiscard]] std::expected<void, core::status> del(std::string_view key) noexcept;

  // TODO(lnikon): Interface tdb. Needs memtable iterator. Should search in both active and immutable?
  void scan(std::string_view range_start_key, std::string_view range_end_key) noexcept;
};

}  // namespace frankie::engine
