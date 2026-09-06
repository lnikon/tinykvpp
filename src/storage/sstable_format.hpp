#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

#include "core/arena.hpp"
#include "core/serialization/buffer_reader.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "core/status.hpp"

// ### VERSION 1 ###
// Wire layout for SSTable on-disk format.
//
// File layout (sequential):
//    [entries]:
//      [u32 ikey_size][ikey bytes][u64 block_offset][u64 block_size]
//    [index region][empty]
//    [bloom region][empty]
//    [footer (sstable_footer_size bytes, fixed)]
//
// Data block:
//   [entries body]
// Each entry within a data block:
//   [u32 internal_key_size][internal_key_bytes][u32 value_size][value_bytes]
//
// Index region:
//  [empty]
// Bloom region:
//  [empty]
// Footer region:
//   [u64 index_offset][u64 index_size]
//   [u64 bloom_offset][u64 bloom_size]
//   [u32 crc32]

// ### VERSION 2 ###
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
//   [u32 internal_key_size][internal_key_bytes][u32 value_size][value_bytes]
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

//
// Internal key
//
struct internal_key final {
  static constexpr std::uint64_t kMetadataSize = 8 + 8 + 1;  // sequence + timestamp + tombstone

  std::string_view user_key_;
  std::uint64_t sequence_;
  std::uint64_t timestamp_;
  bool tombstone_;

  [[nodiscard]] std::expected<void, core::status> encode(core::buffer_writer &writer) const noexcept;

  [[nodiscard]] static std::expected<internal_key, core::status> decode(core::arena &arena, core::buffer_reader &reader) noexcept;

  [[nodiscard]] std::uint64_t bytes_required() const noexcept;
};

//
// KV Entry
//
struct kv_entry final {
  std::string_view key_;
  std::string_view value_;
  std::uint64_t sequence_;
  std::uint64_t timestamp_;
  bool tombstone_;

  [[nodiscard]] internal_key get_internal_key() const noexcept;

  [[nodiscard]] std::uint64_t bytes_count() const noexcept;

  [[nodiscard]] std::expected<void, core::status> encode(core::buffer_writer &writer) const noexcept;

  [[nodiscard]] static std::expected<kv_entry, core::status> decode(core::arena &arena, core::buffer_reader &reader) noexcept;
};

//
// Compression
//
enum class sstable_compression_type : std::uint8_t { none, lz4 /* Not yet supported */ };

//
// SSTable Data Block Header
//
struct sstable_data_block_header final {
  std::uint32_t entry_count_{0};
  std::uint32_t uncompressed_size{0};
  std::uint32_t compressed_size{0};
  sstable_compression_type compression_type{sstable_compression_type::none};
  std::uint8_t pad_[3]{};
  std::uint32_t crc32_{0};

  [[nodiscard]] std::expected<void, core::status> encode(core::buffer_writer &writer) const noexcept;

  [[nodiscard]] static std::expected<sstable_data_block_header, core::status> decode(core::buffer_reader &reader) noexcept;
};
static_assert(sizeof(sstable_data_block_header) == 20, "sstable_data_block_header layout drift");

//
// SSTable Data Block
//
struct sstable_data_block final {
  std::span<std::string_view> entries_;

  [[nodiscard]] std::expected<void, core::status> encode(core::buffer_writer &writer) const noexcept;

  [[nodiscard]] static std::expected<sstable_data_block, core::status> decode(core::buffer_reader &reader, core::arena &arena,
                                                                              const sstable_data_block_header &header) noexcept;

  [[nodiscard]] static std::uint64_t calculate_entry_size(std::string_view ikey, std::string_view value) noexcept;
};

//
// SSTable Index Entry
//
struct sstable_index_entry final {
  std::string_view smallest_key_;
  std::uint64_t data_block_offset_{0};
  std::uint64_t data_block_size_{0};

  [[nodiscard]] std::expected<void, core::status> encode(core::buffer_writer &writer) const noexcept;

  [[nodiscard]] static std::expected<std::span<sstable_index_entry>, core::status> decode(core::buffer_reader &reader, core::arena &arena) noexcept;
};

//
// SSTable Index
//
struct sstable_index final {
  std::span<sstable_index_entry> entries_;

  [[nodiscard]] std::expected<void, core::status> encode(core::buffer_writer &writer) const noexcept;

  [[nodiscard]] static std::expected<std::span<sstable_index_entry>, core::status> decode(core::buffer_reader &reader, core::arena &arena) noexcept;
};

//
// SSTable Footer
//
struct sstable_footer final {
  static constexpr std::uint32_t kFooterSize = 5 * sizeof(std::uint32_t);

  std::uint32_t index_offset_{0};
  std::uint32_t index_size_{0};
  std::uint32_t bloom_offset_{0};
  std::uint32_t bloom_size_{0};
  std::uint32_t crc32_{0};

  [[nodiscard]] std::expected<void, core::status> encode(core::buffer_writer &writer) const noexcept;

  [[nodiscard]] static std::expected<sstable_footer, core::status> decode(core::buffer_reader &reader) noexcept;
};

}  // namespace frankie::storage
