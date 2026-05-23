#pragma once

#include <sys/types.h>
#include <cstdint>
#include <limits>

// Wire layout for SSTable on-disk format. POD only — no logic.
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

// - data_block
// -- data_block_header
// --- entry_count: u32
// --- compression_type: u8
// --- min_sequence: u32
// --- max_sequence: u32
// --- min_key: u32
// --- max_key: u32
// -- data_block_body
// --- [key_size:u32 key:@key_size value_size:u32 value:@value_len tombstone:u8 sequence:u32 timestamp:u32]
// - index
// -- [key_size:u32 key:@key_size block_offset:u32]
// - bloom_filter
// *** no support for blooms yet ***
// - footer
// -- index_offset:u32
// -- bloom_offset:u32

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

struct sstable_footer final {
  std::uint32_t index_offset_{0};
  std::uint32_t index_size_{0};
  std::uint32_t bloom_offset_{0};
  std::uint32_t bloom_size_{0};
  std::uint32_t crc32_{0};
};

}  // namespace frankie::storage
