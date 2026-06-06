#pragma once

#include <cstdint>

#include "core/arena.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "core/status.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

inline constexpr std::uint32_t kSSTableArenaCapacity = 1024 * 1024;

struct sstable_writer_config final {
  // Size of each data block (excluding header) in bytes.
  std::uint32_t target_block_size_{4096};
};

// Block-based SST writer.
//
// Each append populates currently active data block. On each append a index is populated with data from all data
// blocks.
//
// Data block uses an arena to buffer up entries. When data block is ready, it is written into the backing file, and
// arena is reset, but not freed.
//
// Index uses arena that is persistent during the lifetime of the writer.
class sstable_writer final {
 public:
  sstable_writer() = default;
  sstable_writer(const sstable_writer &) = delete;
  sstable_writer &operator=(const sstable_writer &) = delete;
  sstable_writer(sstable_writer &&) = default;
  sstable_writer &operator=(sstable_writer &&) = default;
  ~sstable_writer() noexcept;

  // Creation.
  [[nodiscard]] static std::expected<sstable_writer, core::status> create(sstable_writer_config config) noexcept;

  // Data block.
  [[nodiscard]] std::expected<void, core::status> append(std::string_view ikey, std::string_view value) noexcept;

  [[nodiscard]] bool is_data_block_complete() const noexcept;

  [[nodiscard]] std::expected<std::string_view, core::status> get_data_block() noexcept;

  [[nodiscard]] std::expected<void, core::status> record_data_block(std::uint64_t offset, std::uint64_t size) noexcept;

  [[nodiscard]] std::uint32_t get_data_block_size() const noexcept;

  // Index.
  [[nodiscard]] std::expected<std::string_view, core::status> get_index() noexcept;

  // Footer.
  [[nodiscard]] std::expected<std::string_view, core::status> get_footer(sstable_footer footer) noexcept;

 private:
  struct data_block_state {
    // TODO(lnikon): std::byte instead of a char*.
    core::arena arena_;
    char *data_block_body_{nullptr};
    std::uint32_t data_block_size_{0};
    std::uint32_t data_block_capacity_{0};
    std::uint32_t entry_count_{0};
    core::buffer_writer buffer_writer_;
  };

  struct index_entry {
    // Relative to the beginning of the file.
    std::uint64_t data_block_offset_{0};
    std::uint64_t data_block_size_{0};
    // Stored in index dedicated arena.
    std::string_view smallest_key_;
  };

  struct index_entries_state {
    // One index entry per data block.
    core::arena arena_;
    index_entry *index_entries_{nullptr};
    std::uint32_t index_entries_size_{0};
    std::uint32_t index_entries_capacity_{0};
    std::uint32_t entry_count_{0};
  };

  sstable_writer_config config_;

  // General purpose allocator.
  core::arena gpa_arena_;
  // Per data block state.
  data_block_state current_block_state_{};
  // Index accounting for all data blocks.
  index_entries_state index_entries_state_{};
  // Count of all entries from all data blocks.
  std::uint64_t total_entry_count_{};
};

}  // namespace frankie::storage
