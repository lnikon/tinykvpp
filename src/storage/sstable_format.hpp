#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

#include "core/arena.hpp"
#include "core/serialization/buffer_reader.hpp"
#include "core/status.hpp"

// Wire layout for SSTable on-disk format.
//
// File layout (sequential):
//   [data block 0]
//   [data block 1]
//   ...
//   [data block N-1]
//   [index region]
//   [bloom region]
//   [footer (sstable_footer_size bytes, fixed)]
//
// Data block:
//   [sstable_data_block_header (sstable_data_block_header_size bytes)]
//   [entries body]
// Each entry within a data block:
//   [u32 ikey_size][ikey bytes][u32 value_size][value bytes]
//
// Index region:
//   [u32 entry_count]
//   per entry:
//     [u32 ikey_size][ikey bytes][u64 block_offset][u64 block_size]
//
// Bloom region:
//   [u64 bits][u64 hashes][bits/8 bytes of bit array]
//
// Footer region:
//   [u64 index_offset][u64 index_size]
//   [u64 bloom_offset][u64 bloom_size]
//   [u32 crc32]

namespace frankie::storage {

enum class sstable_compression_type : std::uint8_t { none, lz4 };

struct sstable_data_block_header final {
  std::uint32_t entry_count_{0};
  std::uint32_t uncompressed_size{0};
  std::uint32_t compressed_size{0};
  sstable_compression_type compression_type{sstable_compression_type::none};
  std::uint8_t pad_[3]{};
  std::uint32_t crc32_{0};
};
static_assert(sizeof(sstable_data_block_header) == 20, "sstable_data_block_header layout drift");

struct index_entry final {
  std::string_view smallest_key_;
  std::uint64_t data_block_offset_{0};
  std::uint64_t data_block_size_{0};
};

[[nodiscard]] std::expected<std::span<index_entry>, core::status> decode_index(core::buffer_reader &reader,
                                                                               core::arena &arena);

struct sstable_footer final {
  std::uint32_t index_offset_{0};
  std::uint32_t index_size_{0};
  std::uint32_t bloom_offset_{0};
  std::uint32_t bloom_size_{0};
  std::uint32_t crc32_{0};
};

[[nodiscard]] std::expected<sstable_footer, core::status> decode_footer(core::buffer_reader &reader) noexcept;

// TODO(lnikon): Replace with sizeof(sst_footer) when bloom and crc32 are ready.
inline constexpr std::uint32_t kFooterSize = 2 * sizeof(std::uint32_t);

}  // namespace frankie::storage
