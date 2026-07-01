#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string_view>

#include "core/arena.hpp"
#include "core/status.hpp"
#include "storage/memtable.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

struct segment final {
  // Arena for index allocation.
  core::arena index_arena_;
  // Sorted span of index entries over arena.
  std::span<index_entry> index_entries_;
  // Path to the segment relative to the something idk.
  std::filesystem::path path_;
  // Compaction sh]ould have a way to track in-use segments to avoid prematurely removing them.
  std::uint32_t reference_count_{0};
  // Metadata.
  sstable_footer footer_{};

  [[nodiscard]] static std::expected<segment, core::status> create(std::filesystem::path path) noexcept;

  [[nodiscard]] std::expected<kv_entry, core::status> get_record(std::string_view key) noexcept;

 private:
  [[nodiscard]] std::optional<index_entry> get_record_offset(std::string_view key) const noexcept;
};

}  // namespace frankie::storage
