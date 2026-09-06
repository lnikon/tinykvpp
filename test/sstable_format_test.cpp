#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "core/arena.hpp"
#include "core/serialization/buffer_reader.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "storage/sstable_format.hpp"

using namespace frankie;
using namespace frankie::storage;

TEST(SstableFormatTest, FooterRoundTrip) {
  constexpr std::uint32_t index_offset = 0x11223344u;
  constexpr std::uint32_t index_size = 0x55667788u;

  std::array<std::byte, 8> buf{};
  auto w = core::buffer_writer::create(buf);
  (void)w.write<std::uint32_t>(index_offset).write<std::uint32_t>(index_size);
  ASSERT_FALSE(w.error().has_value());

  core::buffer_reader r(buf);
  auto f = decode_footer(r);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(f->index_offset_, index_offset);
  EXPECT_EQ(f->index_size_, index_size);
}

TEST(SstableFormatTest, IndexRoundTrip) {
  constexpr std::uint32_t kEntryCount = 3;

  // Keys must outlive the test body: decoded smallest_key_ is a string_view
  // into the encode buffer, and write_string consumes a string_view.
  const std::array<std::string, kEntryCount> keys = {"alpha", "bravo", "charlie"};
  const std::array<std::uint64_t, kEntryCount> offsets = {10u, 200u, 3000u};
  const std::array<std::uint64_t, kEntryCount> sizes = {7u, 77u, 777u};

  std::array<std::byte, 256> buf{};
  auto w = core::buffer_writer::create(buf);
  (void)w.write<std::uint32_t>(kEntryCount);
  for (std::uint32_t i = 0; i < kEntryCount; ++i) {
    (void)w.write_string(keys[i]).write<std::uint64_t>(offsets[i]).write<std::uint64_t>(sizes[i]);
  }
  ASSERT_FALSE(w.error().has_value());

  core::buffer_reader r(buf);
  core::arena arena = core::arena::create(64ULL * 1024);
  auto idx = decode_index(r, arena);
  ASSERT_TRUE(idx.has_value());
  EXPECT_EQ(idx->size(), kEntryCount);

  for (std::uint32_t i = 0; i < kEntryCount; ++i) {
    const auto &entry = (*idx)[i];
    EXPECT_EQ(entry.smallest_key_, keys[i]);
    EXPECT_EQ(entry.data_block_offset_, offsets[i]);
    EXPECT_EQ(entry.data_block_size_, sizes[i]);
  }
}

TEST(SstableFormatTest, InternalKeyBufferReaderRoundTrip) {
  constexpr std::uint64_t sequence = 42u;
  constexpr std::uint64_t timestamp = 12345u;
  constexpr bool tombstone = true;

  // user_key must outlive the test body: decoded user_key_ is a string_view
  // into the encode buffer.
  const std::string user_key = "hello-world";

  std::array<std::byte, 64> buf{};
  std::span<std::byte> buf_span(buf);
  auto w = core::buffer_writer::create(&buf_span);
  const internal_key ik{.user_key_ = user_key, .sequence_ = sequence, .timestamp_ = timestamp, .tombstone_ = tombstone};
  ASSERT_TRUE(ik.encode(w).has_value());

  core::buffer_reader r(std::span<const std::byte>(buf).first(w.cursor()));
  auto decoded = internal_key::decode(r);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->user_key_, user_key);
  EXPECT_EQ(decoded->sequence_, sequence);
  EXPECT_EQ(decoded->timestamp_, timestamp);
  EXPECT_EQ(decoded->tombstone_, tombstone);
}

TEST(SstableFormatTest, DataBlockHeaderSize) {
  EXPECT_EQ(sizeof(frankie::storage::sstable_data_block_header), 20u);
}
