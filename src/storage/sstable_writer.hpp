#pragma once

#include <concepts>
#include <expected>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

#include "core/arena.hpp"
#include "core/fs.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "core/status.hpp"
#include "storage/sstable_format.hpp"

namespace frankie::storage {

// ===
// high level idea of how to decouple memtable from sstable_writer

using kv_pair = std::pair<std::string_view, std::string_view>;

template <class R>
concept kv_source = std::ranges::input_range<R> && std::convertible_to<std::ranges::range_reference_t<R>, kv_pair>;

//===

struct sstable_writer_const final {
  static const std::uint64_t default_target_block_size_{4096};
  static const std::uint64_t default_max_entry_size_{4096};
  static const std::uint64_t default_max_key_size_{1024};
  static const sstable_compression_type default_compression_{sstable_compression_type::none};
};

struct sstable_writer_options final {
  std::uint64_t target_block_size_{sstable_writer_const::default_target_block_size_};
  std::uint64_t max_entry_size_{sstable_writer_const::default_max_entry_size_};
  std::uint64_t max_key_size_{sstable_writer_const::default_max_key_size_};
  std::uint64_t max_entry_count_{0};
  std::uint64_t block_in_flight_{1};
  sstable_compression_type compression_{sstable_writer_const::default_compression_};
};

// Holds upper bounds on capacity, count, etc...
// Derived from @sstable_writer_options.
// Inspired by TigetBeetle.
struct sstable_allocation_config final {
  std::uint64_t block_body_max_{0};
  std::uint64_t scratch_bytes_{0};
  std::uint64_t entries_per_block_min_{0};
  std::uint64_t data_block_count_max_{0};
  std::uint64_t index_entries_bytes_{0};
  std::uint64_t index_key_blob_bytes_{0};
  std::uint64_t arena_bytes_{0};

  [[nodiscard]] static std::expected<sstable_allocation_config, core::status> from(const sstable_writer_options &options) noexcept {
    if (options.max_entry_count_ == 0 || options.block_in_flight_ == 0 || options.max_entry_size_ == 0) {
      return core::unexpected(core::status_code::invalid_argument);
    }

    sstable_allocation_config alloc;
    alloc.block_body_max_ = options.target_block_size_ + options.max_entry_size_;

    // In worst case could be one entry per block.
    alloc.entries_per_block_min_ = alloc.block_body_max_ / options.max_entry_size_;
    alloc.data_block_count_max_ = (options.max_entry_count_ + alloc.entries_per_block_min_ - 1) / alloc.entries_per_block_min_;

    alloc.scratch_bytes_ = options.block_in_flight_ * (sizeof(sstable_data_block_header) + alloc.block_body_max_);
    alloc.index_entries_bytes_ = alloc.data_block_count_max_ * sizeof(sstable_index_entry);
    alloc.index_key_blob_bytes_ = alloc.data_block_count_max_ * options.max_key_size_;

    alloc.arena_bytes_ =
        alloc.scratch_bytes_ + alloc.index_entries_bytes_ + alloc.index_key_blob_bytes_ + sstable_footer::kFooterSize + (3 * core::kBlockAlignment);

    return alloc;
  }
};

class sstable_writer final {
  static constexpr std::uint64_t kMaxIndexEntriesCount = 4096;

  sstable_writer_options options_;
  sstable_allocation_config allocation_config;
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
  std::span<sstable_index_entry> index_entries_;
  std::span<char> index_key_blob_;

 public:
  sstable_writer() = default;
  sstable_writer(const sstable_writer &) = delete;
  sstable_writer &operator=(const sstable_writer &) = delete;
  sstable_writer(sstable_writer &&) = default;
  sstable_writer &operator=(sstable_writer &&) = default;
  ~sstable_writer() noexcept;

  template <kv_source R>
  [[nodiscard]] static std::expected<void, core::status> flush(R &&source, core::random_access_file file, sstable_writer_options options);

  [[nodiscard]] static std::expected<sstable_writer, core::status> create(core::random_access_file file, sstable_writer_options options) noexcept;

  [[nodiscard]] std::expected<void, core::status> add(std::string_view encoded_internal_key, std::string_view value) noexcept;

  [[nodiscard]] std::expected<void, core::status> finish() noexcept;
};

template <kv_source R>
std::expected<void, core::status> sstable_writer::flush(R &&source,  // NOLINT(cppcoreguidelines-missing-std-forward)
                                                        core::random_access_file file, sstable_writer_options options) {
  auto writer_or = sstable_writer::create(std::move(file), options);
  if (!writer_or) {
    return core::unexpected(writer_or.error());
  }

  for (auto &&[encoded_internal_key, value] : source) {
    if (const auto status = writer_or->add(encoded_internal_key, value); !status) {
      return core::unexpected(status.error());
    }
  }

  if (const auto status = writer_or->finish(); !status) {
    return core::unexpected(status.error());
  }

  return {};
}

}  // namespace frankie::storage
