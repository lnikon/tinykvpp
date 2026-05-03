#include "engine/wal.hpp"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>
#include <vector>

#include "core/crc32.hpp"
#include "core/scratch_arena.hpp"

using namespace frankie::engine;
using namespace frankie::core;

// Helper to read a little-endian value from a buffer at a given offset.
template <typename T>
static T read_le(const char *buf, std::size_t offset) {
  T val{};
  std::memcpy(&val, buf + offset, sizeof(T));
  return val;
}

class WalEntryTest : public ::testing::Test {
 protected:
  scratch_arena arena_;
};

TEST_F(WalEntryTest, EncodePutEntry) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 42,
      .key_ = "hello",
      .value_ = "world",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);

  const std::uint32_t expected_size = wal_entry::kMetadataSize + 5 + 5;
  ASSERT_EQ(encoded.size(), expected_size);

  const char *buf = encoded.data();

  // record_len = total - sizeof(record_len) - sizeof(crc32)
  const std::uint32_t record_len = read_le<std::uint32_t>(buf, 0);
  EXPECT_EQ(record_len, expected_size - 8);

  // operation
  EXPECT_EQ(static_cast<wal_operation>(buf[8]), wal_operation::put);

  // sequence
  EXPECT_EQ(read_le<std::uint64_t>(buf, 9), 42u);

  // tombstone
  EXPECT_EQ(static_cast<std::uint8_t>(buf[17]), 0);

  // key_len
  EXPECT_EQ(read_le<std::uint32_t>(buf, 18), 5u);

  // value_len
  EXPECT_EQ(read_le<std::uint32_t>(buf, 22), 5u);

  // key bytes
  EXPECT_EQ(std::string_view(buf + 26, 5), "hello");

  // value bytes
  EXPECT_EQ(std::string_view(buf + 31, 5), "world");
}

TEST_F(WalEntryTest, EncodeDeleteEntry) {
  wal_entry entry{
      .operation_ = wal_operation::del,
      .sequence_ = 100,
      .key_ = "removed",
      .value_ = "",
      .tombstone_ = 1,
  };

  auto encoded = entry.encode(arena_);

  const std::uint32_t expected_size = wal_entry::kMetadataSize + 7 + 0;
  ASSERT_EQ(encoded.size(), expected_size);

  const char *buf = encoded.data();

  EXPECT_EQ(static_cast<wal_operation>(buf[8]), wal_operation::del);
  EXPECT_EQ(read_le<std::uint64_t>(buf, 9), 100u);
  EXPECT_EQ(static_cast<std::uint8_t>(buf[17]), 1);
  EXPECT_EQ(read_le<std::uint32_t>(buf, 18), 7u);
  EXPECT_EQ(read_le<std::uint32_t>(buf, 22), 0u);
  EXPECT_EQ(std::string_view(buf + 26, 7), "removed");
}

TEST_F(WalEntryTest, EncodeCRC32IsValid) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);
  const char *buf = encoded.data();

  // CRC32 is at offset 4, computed over everything from offset 8 onward.
  const std::uint32_t stored_crc = read_le<std::uint32_t>(buf, 4);
  const std::uint32_t record_offset = 8;

  const std::uint32_t computed_crc =
      crc32{}
          .update({reinterpret_cast<const std::byte *>(buf) + record_offset, encoded.size() - record_offset})
          .finalize();

  EXPECT_EQ(stored_crc, computed_crc);
}

TEST_F(WalEntryTest, EncodeEmptyKeyAndValue) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 0,
      .key_ = "",
      .value_ = "",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);
  EXPECT_EQ(encoded.size(), wal_entry::kMetadataSize);

  const char *buf = encoded.data();
  EXPECT_EQ(read_le<std::uint32_t>(buf, 18), 0u);  // key_len
  EXPECT_EQ(read_le<std::uint32_t>(buf, 22), 0u);  // value_len
}

TEST_F(WalEntryTest, EncodeResetsScratchArena) {
  wal_entry entry1{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "first",
      .value_ = "value1",
      .tombstone_ = 0,
  };

  wal_entry entry2{
      .operation_ = wal_operation::del,
      .sequence_ = 2,
      .key_ = "second",
      .value_ = "",
      .tombstone_ = 1,
  };

  // Encode twice — the second encode should produce a valid independent result.
  [[maybe_unused]] auto encoded1 = entry1.encode(arena_);
  auto encoded2 = entry2.encode(arena_);

  const char *buf = encoded2.data();
  EXPECT_EQ(static_cast<wal_operation>(buf[8]), wal_operation::del);
  EXPECT_EQ(read_le<std::uint64_t>(buf, 9), 2u);
}

TEST_F(WalEntryTest, DecodeRoundTripPut) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 42,
      .key_ = "hello",
      .value_ = "world",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);
  auto decoded = wal_entry::decode(encoded);
  ASSERT_TRUE(decoded.has_value());

  EXPECT_EQ(decoded->operation_, wal_operation::put);
  EXPECT_EQ(decoded->sequence_, 42u);
  EXPECT_EQ(decoded->tombstone_, 0);
  EXPECT_EQ(decoded->key_, "hello");
  EXPECT_EQ(decoded->value_, "world");
}

TEST_F(WalEntryTest, DecodeRoundTripDelete) {
  wal_entry entry{
      .operation_ = wal_operation::del,
      .sequence_ = 100,
      .key_ = "removed",
      .value_ = "",
      .tombstone_ = 1,
  };

  auto encoded = entry.encode(arena_);
  auto decoded = wal_entry::decode(encoded);
  ASSERT_TRUE(decoded.has_value());

  EXPECT_EQ(decoded->operation_, wal_operation::del);
  EXPECT_EQ(decoded->sequence_, 100u);
  EXPECT_EQ(decoded->tombstone_, 1);
  EXPECT_EQ(decoded->key_, "removed");
  EXPECT_EQ(decoded->value_, "");
}

TEST_F(WalEntryTest, DecodeRoundTripEmptyKeyAndValue) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 0,
      .key_ = "",
      .value_ = "",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);
  auto decoded = wal_entry::decode(encoded);
  ASSERT_TRUE(decoded.has_value());

  EXPECT_EQ(decoded->operation_, wal_operation::put);
  EXPECT_EQ(decoded->sequence_, 0u);
  EXPECT_EQ(decoded->tombstone_, 0);
  EXPECT_EQ(decoded->key_, "");
  EXPECT_EQ(decoded->value_, "");
}

TEST_F(WalEntryTest, DecodeRejectsCorruptedCRC) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);

  // Copy into a mutable buffer and flip a byte in the CRC field (offset 4).
  std::string corrupted(encoded);
  corrupted[4] = static_cast<char>(~corrupted[4]);

  std::string_view view(corrupted);
  auto decoded = wal_entry::decode(view);
  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error().code_, status_code::corrupted);
}

TEST_F(WalEntryTest, DecodeRejectsCorruptedPayload) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);

  // Flip a byte in the payload region (offset 10, inside the sequence field).
  std::string corrupted(encoded);
  corrupted[10] = static_cast<char>(~corrupted[10]);

  std::string_view view(corrupted);
  auto decoded = wal_entry::decode(view);
  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error().code_, status_code::corrupted);
}

TEST_F(WalEntryTest, DecodeRejectsKeyValueOverflowingRecordLen) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);
  std::string buf(encoded);

  // Inflate key_len (at offset 18) so key_len + value_len exceeds record_len.
  const std::uint32_t bogus_key_len = 9999;
  std::memcpy(buf.data() + 18, &bogus_key_len, sizeof(bogus_key_len));

  // Recompute CRC over the (now-corrupt) payload so the checksum passes.
  const std::uint32_t record_offset = 8;
  const std::uint32_t record_len = read_le<std::uint32_t>(buf.data(), 0);
  const std::uint32_t new_crc =
      crc32{}
          .update({reinterpret_cast<const std::byte *>(buf.data()) + record_offset, record_len})
          .finalize();
  std::memcpy(buf.data() + 4, &new_crc, sizeof(new_crc));

  std::string_view view(buf);
  auto decoded = wal_entry::decode(view);
  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error().code_, status_code::corrupted);
}

TEST_F(WalEntryTest, DecodeEmptyBufferReturnsEof) {
  std::string_view empty;
  auto decoded = wal_entry::decode(empty);
  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error().code_, status_code::eof);
}

TEST_F(WalEntryTest, DecodeTruncatedBufferReturnsCorrupted) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "key",
      .value_ = "val",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);

  // Truncate to just the header — missing key/value bytes. Non-empty but
  // shorter than kMetadataSize → corrupted (caller can no longer parse a
  // header).
  std::string_view truncated(encoded.data(), wal_entry::kMetadataSize - 1);

  auto decoded = wal_entry::decode(truncated);
  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error().code_, status_code::corrupted);
}

TEST_F(WalEntryTest, DecodeAdvancesViewOnSuccess) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);

  auto decoded = wal_entry::decode(encoded);
  ASSERT_TRUE(decoded.has_value());
  // A single-entry buffer is fully consumed after a successful decode.
  EXPECT_EQ(encoded.size(), 0u);
}

TEST_F(WalEntryTest, DecodeLeavesViewUntouchedOnFailure) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };

  auto encoded = entry.encode(arena_);
  std::string corrupted(encoded);
  corrupted[4] = static_cast<char>(~corrupted[4]);  // flip a CRC byte

  std::string_view view(corrupted);
  const auto *data_before = view.data();
  const auto size_before = view.size();

  auto decoded = wal_entry::decode(view);
  EXPECT_FALSE(decoded.has_value());
  // Failed decode must not advance the view — the caller needs the original
  // bytes to report/recover from the corruption.
  EXPECT_EQ(view.data(), data_before);
  EXPECT_EQ(view.size(), size_before);
}

TEST_F(WalEntryTest, DecodeAfterFullConsumeReturnsEof) {
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };
  auto encoded = entry.encode(arena_);
  ASSERT_TRUE(wal_entry::decode(encoded).has_value());
  // View is fully consumed — next decode reports eof, not corrupted.
  auto next = wal_entry::decode(encoded);
  ASSERT_FALSE(next.has_value());
  EXPECT_EQ(next.error().code_, status_code::eof);
}

TEST_F(WalEntryTest, DecodeTwoEntriesFromSingleBuffer) {
  wal_entry e1{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "a",
      .value_ = "b",
      .tombstone_ = 0,
  };
  wal_entry e2{
      .operation_ = wal_operation::del,
      .sequence_ = 2,
      .key_ = "cc",
      .value_ = "",
      .tombstone_ = 1,
  };

  // Concatenate the two encoded entries into a stable backing buffer. arena_
  // gets reset between encodes, so we must copy the bytes out before the
  // second encode overwrites them.
  std::string combined;
  {
    auto enc1 = e1.encode(arena_);
    combined.append(enc1);
  }
  {
    auto enc2 = e2.encode(arena_);
    combined.append(enc2);
  }

  std::string_view view(combined);

  auto d1 = wal_entry::decode(view);
  ASSERT_TRUE(d1.has_value());
  EXPECT_EQ(d1->sequence_, 1u);
  EXPECT_EQ(d1->key_, "a");
  EXPECT_EQ(d1->value_, "b");

  auto d2 = wal_entry::decode(view);
  ASSERT_TRUE(d2.has_value());
  EXPECT_EQ(d2->sequence_, 2u);
  EXPECT_EQ(d2->key_, "cc");
  EXPECT_EQ(d2->tombstone_, 1);

  // View fully consumed — further decodes return nullopt.
  EXPECT_EQ(view.size(), 0u);
  EXPECT_FALSE(wal_entry::decode(view).has_value());
}

// --- wal_writer tests (use real files via tmp directory) ---

class WalWriterTest : public ::testing::Test {
 protected:
  std::filesystem::path tmp_dir_;

  void SetUp() override {
    tmp_dir_ = std::filesystem::temp_directory_path() / "frankie_wal_test";
    std::filesystem::create_directories(tmp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(tmp_dir_); }
};

TEST_F(WalWriterTest, OpenCreatesFile) {
  auto path = tmp_dir_ / "test.wal";
  auto writer = wal_writer::open(path, 1024);
  ASSERT_TRUE(writer.has_value());
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_TRUE(writer->close());
}

TEST_F(WalWriterTest, OpenInvalidPathFails) {
  auto path = tmp_dir_ / "nonexistent_dir" / "deep" / "test.wal";
  auto writer = wal_writer::open(path, 1024);
  EXPECT_FALSE(writer.has_value());
}

TEST_F(WalWriterTest, AppendWritesData) {
  auto path = tmp_dir_ / "append.wal";
  auto writer = wal_writer::open(path, 4096);
  ASSERT_TRUE(writer.has_value());

  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "key1",
      .value_ = "val1",
      .tombstone_ = 0,
  };

  EXPECT_TRUE(writer->append(entry));
  EXPECT_TRUE(writer->close());

  // Verify file is non-empty and has the expected size.
  const auto file_size = std::filesystem::file_size(path);
  EXPECT_EQ(file_size, wal_entry::kMetadataSize + 4 + 4);
}

TEST_F(WalWriterTest, AppendMultipleEntries) {
  auto path = tmp_dir_ / "multi.wal";
  auto writer = wal_writer::open(path, 4096);
  ASSERT_TRUE(writer.has_value());

  wal_entry e1{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "a",
      .value_ = "b",
      .tombstone_ = 0,
  };
  wal_entry e2{
      .operation_ = wal_operation::del,
      .sequence_ = 2,
      .key_ = "c",
      .value_ = "",
      .tombstone_ = 1,
  };

  EXPECT_TRUE(writer->append(e1));
  EXPECT_TRUE(writer->append(e2));
  EXPECT_TRUE(writer->close());

  const auto expected_size = (wal_entry::kMetadataSize + 1 + 1) + (wal_entry::kMetadataSize + 1 + 0);
  EXPECT_EQ(std::filesystem::file_size(path), expected_size);
}

TEST_F(WalWriterTest, AppendedDataHasValidCRC) {
  auto path = tmp_dir_ / "crc.wal";
  auto writer = wal_writer::open(path, 4096);
  ASSERT_TRUE(writer.has_value());

  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 7,
      .key_ = "test",
      .value_ = "data",
      .tombstone_ = 0,
  };

  EXPECT_TRUE(writer->append(entry));
  EXPECT_TRUE(writer->close());

  // Read the file back and verify CRC.
  const auto file_size = std::filesystem::file_size(path);
  std::vector<char> contents(file_size);
  int fd = ::open(path.c_str(), O_RDONLY);
  ASSERT_NE(fd, -1);
  ASSERT_EQ(::read(fd, contents.data(), file_size), static_cast<ssize_t>(file_size));
  ::close(fd);
  const char *buf = contents.data();

  const std::uint32_t stored_crc = read_le<std::uint32_t>(buf, 4);
  const std::uint32_t record_offset = 8;

  const std::uint32_t computed_crc =
      crc32{}
          .update({reinterpret_cast<const std::byte *>(buf) + record_offset, contents.size() - record_offset})
          .finalize();

  EXPECT_EQ(stored_crc, computed_crc);
}

TEST_F(WalWriterTest, SyncSucceeds) {
  auto path = tmp_dir_ / "sync.wal";
  auto writer = wal_writer::open(path, 1024);
  ASSERT_TRUE(writer.has_value());
  EXPECT_TRUE(writer->sync());
}

TEST_F(WalWriterTest, MoveAssignmentClosesOldFd) {
  auto path1 = tmp_dir_ / "move1.wal";
  auto path2 = tmp_dir_ / "move2.wal";

  auto writer1 = wal_writer::open(path1, 1024);
  ASSERT_TRUE(writer1.has_value());

  auto writer2 = wal_writer::open(path2, 1024);
  ASSERT_TRUE(writer2.has_value());

  // Append to writer1 before it gets overwritten.
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };
  EXPECT_TRUE(writer1->append(entry));

  // Move-assign writer2 into writer1. This should close writer1's old fd.
  *writer1 = std::move(*writer2);

  // writer1 now owns writer2's old fd — it should still be functional.
  EXPECT_TRUE(writer1->append(entry));
  EXPECT_TRUE(writer1->close());

  // Both files should exist and have data.
  EXPECT_TRUE(std::filesystem::exists(path1));
  EXPECT_TRUE(std::filesystem::exists(path2));
  EXPECT_GT(std::filesystem::file_size(path1), 0u);
  EXPECT_GT(std::filesystem::file_size(path2), 0u);
}

TEST_F(WalWriterTest, MoveConstructionLeavesSourceInert) {
  auto path = tmp_dir_ / "move_ctor.wal";
  auto writer = wal_writer::open(path, 1024);
  ASSERT_TRUE(writer.has_value());

  // Move-construct a new writer from the optional's value.
  wal_writer moved{std::move(*writer)};

  // The moved-to writer should be functional.
  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };
  EXPECT_TRUE(moved.append(entry));
  EXPECT_TRUE(moved.close());

  // The original goes out of scope — must not crash (fd_ is -1).
}

TEST_F(WalWriterTest, TruncateResetsFileToEmpty) {
  auto path = tmp_dir_ / "truncate.wal";
  auto writer = wal_writer::open(path, 4096);
  ASSERT_TRUE(writer.has_value());

  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "key1",
      .value_ = "val1",
      .tombstone_ = 0,
  };

  EXPECT_TRUE(writer->append(entry));
  EXPECT_GT(std::filesystem::file_size(path), 0u);

  EXPECT_TRUE(writer->truncate());
  EXPECT_EQ(std::filesystem::file_size(path), 0u);
}

TEST_F(WalWriterTest, TruncateThenAppendWritesFromBeginning) {
  auto path = tmp_dir_ / "truncate_append.wal";
  auto writer = wal_writer::open(path, 4096);
  ASSERT_TRUE(writer.has_value());

  wal_entry e1{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "before",
      .value_ = "truncate",
      .tombstone_ = 0,
  };
  EXPECT_TRUE(writer->append(e1));

  EXPECT_TRUE(writer->truncate());

  wal_entry e2{
      .operation_ = wal_operation::put,
      .sequence_ = 2,
      .key_ = "after",
      .value_ = "truncate",
      .tombstone_ = 0,
  };
  EXPECT_TRUE(writer->append(e2));

  // File should contain only e2, not e1+e2.
  const auto expected_size = wal_entry::kMetadataSize + 5 + 8;
  EXPECT_EQ(std::filesystem::file_size(path), expected_size);

  // Read back and verify CRC + decode of the single entry.
  std::vector<char> contents(expected_size);
  int fd = ::open(path.c_str(), O_RDONLY);
  ASSERT_NE(fd, -1);
  ASSERT_EQ(::read(fd, contents.data(), expected_size), static_cast<ssize_t>(expected_size));
  ::close(fd);

  std::string_view view(contents.data(), contents.size());
  auto decoded = wal_entry::decode(view);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->sequence_, 2u);
  EXPECT_EQ(decoded->key_, "after");
  EXPECT_EQ(decoded->value_, "truncate");
}

TEST_F(WalWriterTest, TruncateOnEmptyFileSucceeds) {
  auto path = tmp_dir_ / "truncate_empty.wal";
  auto writer = wal_writer::open(path, 4096);
  ASSERT_TRUE(writer.has_value());

  EXPECT_EQ(std::filesystem::file_size(path), 0u);
  EXPECT_TRUE(writer->truncate());
  EXPECT_EQ(std::filesystem::file_size(path), 0u);
}

TEST_F(WalWriterTest, TruncateMultipleTimes) {
  auto path = tmp_dir_ / "truncate_multi.wal";
  auto writer = wal_writer::open(path, 4096);
  ASSERT_TRUE(writer.has_value());

  wal_entry entry{
      .operation_ = wal_operation::put,
      .sequence_ = 1,
      .key_ = "k",
      .value_ = "v",
      .tombstone_ = 0,
  };

  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(writer->append(entry));
    EXPECT_GT(std::filesystem::file_size(path), 0u);
    EXPECT_TRUE(writer->truncate());
    EXPECT_EQ(std::filesystem::file_size(path), 0u);
  }
}

// --- wal_reader tests (use real files via tmp directory) ---

class WalReaderTest : public ::testing::Test {
 protected:
  std::filesystem::path tmp_dir_;

  void SetUp() override {
    tmp_dir_ = std::filesystem::temp_directory_path() / "frankie_wal_reader_test";
    std::filesystem::create_directories(tmp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(tmp_dir_); }

  // Helper: write `entries` to `path` via a fresh wal_writer and close it.
  static void write_wal(const std::filesystem::path &path, const std::vector<wal_entry> &entries) {
    auto writer = wal_writer::open(path, 4096);
    ASSERT_TRUE(writer.has_value());
    for (const auto &e : entries) {
      EXPECT_TRUE(writer->append(e));
    }
    EXPECT_TRUE(writer->close());
  }
};

TEST_F(WalReaderTest, OpenNonexistentFileFails) {
  auto reader = wal_reader::open(tmp_dir_ / "does_not_exist.wal");
  ASSERT_FALSE(reader.has_value());
  // Missing file (ENOENT) maps to not_found — distinct from eof (existing but
  // empty file) and from io_error (real I/O failure). Recovery uses the
  // not_found / eof distinction to treat both as a clean start while still
  // surfacing real I/O failures.
  EXPECT_EQ(reader.error().code_, status_code::not_found);
}

TEST_F(WalReaderTest, OpenEmptyFileReturnsEof) {
  // A freshly-opened-then-closed wal_writer leaves a 0-byte file behind. The
  // reader reports eof so the engine can distinguish "nothing to recover" from
  // a real I/O failure.
  auto path = tmp_dir_ / "empty.wal";
  {
    auto writer = wal_writer::open(path, 1024);
    ASSERT_TRUE(writer.has_value());
    EXPECT_TRUE(writer->close());
  }
  ASSERT_TRUE(std::filesystem::exists(path));
  ASSERT_EQ(std::filesystem::file_size(path), 0u);

  auto reader = wal_reader::open(path);
  ASSERT_FALSE(reader.has_value());
  EXPECT_EQ(reader.error().code_, status_code::eof);
}

TEST_F(WalReaderTest, ReadSingleEntryRoundTrip) {
  auto path = tmp_dir_ / "single.wal";
  write_wal(path, {wal_entry{
                      .operation_ = wal_operation::put,
                      .sequence_ = 7,
                      .key_ = "hello",
                      .value_ = "world",
                      .tombstone_ = 0,
                  }});

  auto reader = wal_reader::open(path);
  ASSERT_TRUE(reader.has_value());

  auto decoded = reader->read();
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->operation_, wal_operation::put);
  EXPECT_EQ(decoded->sequence_, 7u);
  EXPECT_EQ(decoded->key_, "hello");
  EXPECT_EQ(decoded->value_, "world");
  EXPECT_EQ(decoded->tombstone_, 0);

  // Buffer is now fully consumed — further reads report eof (not corrupted)
  // and stay idempotent across repeated calls.
  auto eof1 = reader->read();
  ASSERT_FALSE(eof1.has_value());
  EXPECT_EQ(eof1.error().code_, status_code::eof);
  auto eof2 = reader->read();
  ASSERT_FALSE(eof2.has_value());
  EXPECT_EQ(eof2.error().code_, status_code::eof);
}

TEST_F(WalReaderTest, ReadMultipleEntriesInOrder) {
  auto path = tmp_dir_ / "multi.wal";
  const std::vector<wal_entry> entries = {
      wal_entry{.operation_ = wal_operation::put, .sequence_ = 1, .key_ = "a", .value_ = "1", .tombstone_ = 0},
      wal_entry{.operation_ = wal_operation::put, .sequence_ = 2, .key_ = "bb", .value_ = "22", .tombstone_ = 0},
      wal_entry{.operation_ = wal_operation::del, .sequence_ = 3, .key_ = "a", .value_ = "", .tombstone_ = 1},
  };
  write_wal(path, entries);

  auto reader = wal_reader::open(path);
  ASSERT_TRUE(reader.has_value());

  for (const auto &expected : entries) {
    auto actual = reader->read();
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(actual->operation_, expected.operation_);
    EXPECT_EQ(actual->sequence_, expected.sequence_);
    EXPECT_EQ(actual->key_, expected.key_);
    EXPECT_EQ(actual->value_, expected.value_);
    EXPECT_EQ(actual->tombstone_, expected.tombstone_);
  }

  EXPECT_FALSE(reader->read().has_value());
}

TEST_F(WalReaderTest, ReadStopsAtFirstCorruptedEntry) {
  auto path = tmp_dir_ / "corrupt.wal";
  write_wal(path, {
      wal_entry{.operation_ = wal_operation::put, .sequence_ = 1, .key_ = "a", .value_ = "b", .tombstone_ = 0},
      wal_entry{.operation_ = wal_operation::put, .sequence_ = 2, .key_ = "c", .value_ = "d", .tombstone_ = 0},
  });

  // Flip a byte in the second entry's CRC field. Entry layout:
  //   [record_len u32][crc32 u32][...payload...]
  // so the CRC of the second entry sits at offset (first_entry_size + 4).
  const off_t first_entry_size = wal_entry::kMetadataSize + 1 + 1;
  const off_t second_crc_offset = first_entry_size + sizeof(std::uint32_t);
  {
    int fd = ::open(path.c_str(), O_RDWR);
    ASSERT_NE(fd, -1);
    char byte = 0;
    ASSERT_EQ(::pread(fd, &byte, 1, second_crc_offset), 1);
    byte = static_cast<char>(~byte);
    ASSERT_EQ(::pwrite(fd, &byte, 1, second_crc_offset), 1);
    ::close(fd);
  }

  auto reader = wal_reader::open(path);
  ASSERT_TRUE(reader.has_value());

  // First entry decodes cleanly.
  auto first = reader->read();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->sequence_, 1u);
  EXPECT_EQ(first->key_, "a");

  // Second entry fails CRC → reader reports corrupted (not eof) and the
  // buffer stays parked on the corrupt record so the caller can act on the
  // error.
  auto bad = reader->read();
  ASSERT_FALSE(bad.has_value());
  EXPECT_EQ(bad.error().code_, status_code::corrupted);
  // Repeated reads return the same error — decode leaves the view untouched.
  bad = reader->read();
  ASSERT_FALSE(bad.has_value());
  EXPECT_EQ(bad.error().code_, status_code::corrupted);
}

TEST_F(WalReaderTest, MoveConstructionPreservesReadState) {
  auto path = tmp_dir_ / "move_ctor.wal";
  write_wal(path, {
      wal_entry{.operation_ = wal_operation::put, .sequence_ = 1, .key_ = "k1", .value_ = "v1", .tombstone_ = 0},
      wal_entry{.operation_ = wal_operation::put, .sequence_ = 2, .key_ = "k2", .value_ = "v2", .tombstone_ = 0},
  });

  auto reader = wal_reader::open(path);
  ASSERT_TRUE(reader.has_value());

  // Consume the first entry, then move-construct. The moved-to reader must
  // continue from where the source left off.
  auto first = reader->read();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->sequence_, 1u);

  wal_reader moved{std::move(*reader)};
  auto second = moved.read();
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->sequence_, 2u);

  EXPECT_FALSE(moved.read().has_value());
}

TEST_F(WalReaderTest, MoveAssignmentClosesOldFd) {
  auto path1 = tmp_dir_ / "move_assign_1.wal";
  auto path2 = tmp_dir_ / "move_assign_2.wal";
  write_wal(path1, {wal_entry{
                       .operation_ = wal_operation::put,
                       .sequence_ = 1,
                       .key_ = "x",
                       .value_ = "y",
                       .tombstone_ = 0,
                   }});
  write_wal(path2, {wal_entry{
                       .operation_ = wal_operation::put,
                       .sequence_ = 99,
                       .key_ = "p",
                       .value_ = "q",
                       .tombstone_ = 0,
                   }});

  auto r1 = wal_reader::open(path1);
  auto r2 = wal_reader::open(path2);
  ASSERT_TRUE(r1.has_value());
  ASSERT_TRUE(r2.has_value());

  // Move-assign r2 into r1 — r1's previous fd must be closed by the assign,
  // and the target must now read from path2's content.
  *r1 = std::move(*r2);

  auto entry = r1->read();
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->sequence_, 99u);
  EXPECT_EQ(entry->key_, "p");
}
