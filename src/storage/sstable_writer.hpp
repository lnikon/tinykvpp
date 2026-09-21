#pragma once

#include <expected>
#include <ranges>
#include <string_view>
#include <vector>

#include "core/arena.hpp"
#include "core/fs.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "core/status.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

struct sstable_writer_options final {
  std::uint64_t target_block_size_{4096};  // bytes
  std::uint64_t max_entry_size_{4096};     // bytes
  sstable_compression_type compression_{sstable_compression_type::none};
  std::uint64_t block_in_flight_{1};  // 1 - synchronous; 8 - io_uring
};

class sstable_writer final {
  static constexpr std::uint64_t kMaxIndexEntriesCount = 4096;

  sstable_writer_options options_;
  core::random_access_file file_;

  core::arena arena_;

  // per block state
  std::span<std::byte> data_block_span_;
  core::buffer_writer data_block_span_writer_;
  std::uint64_t data_block_size_bytes_{0};
  std::uint64_t data_block_entries_count_{0};
  std::string data_block_smallest_key_;

  // global data block state
  std::uint64_t data_block_offset_relative_to_zero_{0};

  // one entry per data block
  std::vector<sstable_index_entry> index_entries_;

 public:
  sstable_writer() = default;
  sstable_writer(const sstable_writer &) = delete;
  sstable_writer &operator=(const sstable_writer &) = delete;
  sstable_writer(sstable_writer &&) = default;
  sstable_writer &operator=(sstable_writer &&) = default;
  ~sstable_writer() noexcept;

  // [[nodiscard]] static std::expected<void, core::status> flush(std::ranges::range<std::pair<std::string_view, std::string_view>> range,
  //                                                              core::random_access_file file, sstable_writer_options options);

  [[nodiscard]] static std::expected<sstable_writer, core::status> create(core::random_access_file file, sstable_writer_options options) noexcept;

  [[nodiscard]] std::expected<void, core::status> add(std::string_view encoded_internal_key, std::string_view value) noexcept;

  [[nodiscard]] std::expected<void, core::status> finish() noexcept;
};

}  // namespace frankie::storage
