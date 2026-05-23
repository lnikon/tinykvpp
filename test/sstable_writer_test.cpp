// Regression tests for storage::sstable_writer and the engine's SST
// rotation pipeline. Several tests are expected to fail against the current
// implementation; each one is annotated with the review item it pins.

#include "storage/sstable_writer.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

#include "core/config.hpp"
#include "core/fs.hpp"
#include "core/status.hpp"
#include "engine/engine.hpp"
#include "storage/sstable_format.hpp"

using frankie::core::status_code;
using frankie::storage::sstable_data_block_header;
using frankie::storage::sstable_writer;
using frankie::storage::sstable_writer_config;

namespace {

class SstableWriterTest : public ::testing::Test {
 protected:
  std::filesystem::path tmp_dir_;

  void SetUp() override {
    tmp_dir_ = std::filesystem::temp_directory_path() / "frankie_sstable_writer_test";
    std::filesystem::remove_all(tmp_dir_);
    std::filesystem::create_directories(tmp_dir_);
    std::filesystem::create_directories(tmp_dir_ / "segments");
  }

  void TearDown() override { std::filesystem::remove_all(tmp_dir_); }
};

// ---------------------------------------------------------------------------
// sstable_writer::create
// ---------------------------------------------------------------------------

TEST_F(SstableWriterTest, CreateSucceedsWithDefaults) {
  auto writer = sstable_writer::create(sstable_writer_config{});
  ASSERT_TRUE(writer.has_value());
}

// ---------------------------------------------------------------------------
// append basics — these guard against the P0 nullptr deref in append()
// (see review #1) and the missing data_block_size_ accounting (review #2).
// ---------------------------------------------------------------------------

// REGRESSION (review #1): on the current code, index_entries_ is never
// allocated by create(), so the first append dereferences nullptr at
// index_entries_[size_ - 1]. This test should crash today.
TEST_F(SstableWriterTest, AppendSingleEntryDoesNotCrash) {
  auto writer = sstable_writer::create(sstable_writer_config{});
  ASSERT_TRUE(writer.has_value());

  auto result = writer->append("k", "v");
  EXPECT_TRUE(result.has_value()) << "append must not return an error for a small first entry";
}

TEST_F(SstableWriterTest, AppendManySmallEntriesDoesNotCrash) {
  auto writer = sstable_writer::create(sstable_writer_config{.target_block_size_ = 256});
  ASSERT_TRUE(writer.has_value());

  for (int i = 0; i < 32; ++i) {
    const auto key = "k" + std::to_string(i);
    const auto value = "v" + std::to_string(i);
    auto result = writer->append(key, value);
    ASSERT_TRUE(result.has_value()) << "append #" << i << " failed";
  }
}

// REGRESSION (review #2): data_block_size_ is never incremented in append(),
// so is_data_block_complete() always returns false even after the target is
// exceeded. Engine rotation depends on this signal to flush blocks.
TEST_F(SstableWriterTest, IsDataBlockCompleteFiresOnceTargetExceeded) {
  constexpr std::uint32_t kSmallTarget = 64;
  auto writer = sstable_writer::create(sstable_writer_config{.target_block_size_ = kSmallTarget});
  ASSERT_TRUE(writer.has_value());

  // A handful of small entries that together exceed 64 bytes of payload.
  for (int i = 0; i < 16; ++i) {
    const auto key = "key" + std::to_string(i);
    const std::string value(8, 'v');
    ASSERT_TRUE(writer->append(key, value).has_value());
  }

  EXPECT_TRUE(writer->is_data_block_complete())
      << "after writing >> target_block_size_ bytes, the block must be marked complete";
}

TEST_F(SstableWriterTest, IsDataBlockCompleteFalseBeforeAnyAppend) {
  auto writer = sstable_writer::create(sstable_writer_config{.target_block_size_ = 4096});
  ASSERT_TRUE(writer.has_value());

  EXPECT_FALSE(writer->is_data_block_complete()) << "a freshly created writer must not report a complete block";
}

// ---------------------------------------------------------------------------
// get_data_block — block image must start with a well-formed header
// (review #2: data_block_size_; review #5: implicit string_view truncation
// is observable here because the header contains a null byte).
// ---------------------------------------------------------------------------

TEST_F(SstableWriterTest, GetDataBlockEmitsHeaderWithEntryCount) {
  auto writer = sstable_writer::create(sstable_writer_config{.target_block_size_ = 64});
  ASSERT_TRUE(writer.has_value());

  ASSERT_TRUE(writer->append("aa", "11").has_value());
  ASSERT_TRUE(writer->append("bb", "22").has_value());
  ASSERT_TRUE(writer->append("cc", "33").has_value());

  // Drive the writer until the block reports complete, then grab its image.
  // The current implementation will likely fail before we get here because
  // is_data_block_complete() never flips.
  for (int i = 0; i < 32 && !writer->is_data_block_complete(); ++i) {
    ASSERT_TRUE(writer->append("pad" + std::to_string(i), std::string(8, 'x')).has_value());
  }
  ASSERT_TRUE(writer->is_data_block_complete());

  auto image = writer->get_data_block();
  ASSERT_TRUE(image.has_value()) << "get_data_block must succeed on a complete block";
  ASSERT_GE(image->size(), sizeof(sstable_data_block_header));

  sstable_data_block_header hdr{};
  std::memcpy(&hdr, image->data(), sizeof(hdr));
  EXPECT_GT(hdr.entry_count_, 0u) << "header must report at least one entry";
  EXPECT_GT(hdr.uncompressed_size, 0u) << "header must report a non-zero body size";
  EXPECT_EQ(hdr.uncompressed_size, hdr.compressed_size) << "no compression configured — sizes must match";
}

// REGRESSION (review #10): after get_data_block produces an image the
// writer's block state must be reset so the next append starts a new block.
TEST_F(SstableWriterTest, GetDataBlockResetsBlockStateForNextBlock) {
  auto writer = sstable_writer::create(sstable_writer_config{.target_block_size_ = 32});
  ASSERT_TRUE(writer.has_value());

  // Fill and finalize the first block.
  for (int i = 0; i < 16 && !writer->is_data_block_complete(); ++i) {
    ASSERT_TRUE(writer->append("k" + std::to_string(i), std::string(4, 'v')).has_value());
  }
  ASSERT_TRUE(writer->is_data_block_complete());
  ASSERT_TRUE(writer->get_data_block().has_value());
  // Engine-level handshake — even though no real file is involved here, the
  // index entry should be recorded so the writer's invariants line up.
  ASSERT_TRUE(writer->record_data_block(/*offset=*/0, /*size=*/1).has_value());

  EXPECT_FALSE(writer->is_data_block_complete())
      << "after producing a block image, the writer must start fresh — is_data_block_complete must report false";
}

// ---------------------------------------------------------------------------
// record_data_block — the engine drives this every time it flushes a block;
// the off-by-one between append/record (review #3) makes it fragile.
// ---------------------------------------------------------------------------

TEST_F(SstableWriterTest, RecordDataBlockSucceedsAfterGetDataBlock) {
  auto writer = sstable_writer::create(sstable_writer_config{.target_block_size_ = 32});
  ASSERT_TRUE(writer.has_value());

  for (int i = 0; i < 16 && !writer->is_data_block_complete(); ++i) {
    ASSERT_TRUE(writer->append("k" + std::to_string(i), std::string(4, 'v')).has_value());
  }
  ASSERT_TRUE(writer->is_data_block_complete());
  auto image = writer->get_data_block();
  ASSERT_TRUE(image.has_value());

  auto record_result = writer->record_data_block(/*offset=*/0, image->size());
  EXPECT_TRUE(record_result.has_value()) << "recording a freshly produced block must succeed";
}

// ---------------------------------------------------------------------------
// Engine-level rotation — exercises the full sstable_writer wiring inside
// engine::maybe_rotate_memtable. Pins reviews #4-#9.
// ---------------------------------------------------------------------------

class EngineRotationTest : public ::testing::Test {
 protected:
  std::filesystem::path tmp_dir_;

  void SetUp() override {
    tmp_dir_ = std::filesystem::temp_directory_path() / "frankie_engine_rotation_test";
    std::filesystem::remove_all(tmp_dir_);
    std::filesystem::create_directories(tmp_dir_);
    std::filesystem::create_directories(tmp_dir_ / "segments");
  }

  void TearDown() override { std::filesystem::remove_all(tmp_dir_); }

  [[nodiscard]] frankie::core::config make_config(std::uint64_t memtable_capacity = 512,
                                                  std::uint64_t wal_capacity = 4 * 1024 * 1024) const {
    return frankie::core::config{
        .root_dir_path = tmp_dir_,
        .wal_path = tmp_dir_ / "test.wal",
        .sstable_dir_path = tmp_dir_ / "segments",
        .wal_capacity = wal_capacity,
        .memtable_capacity = memtable_capacity,
    };
  }
};

// REGRESSION (review #6): sstable_id_ is never incremented and never
// appended to the path. Second rotation must succeed, not fail with EEXIST.
TEST_F(EngineRotationTest, TwoConsecutiveRotationsBothSucceed) {
  auto eng = frankie::engine::engine::create(make_config(/*memtable_capacity=*/512));
  ASSERT_TRUE(eng.has_value());

  const std::string big(400, 'x');

  // First rotation: filler must succeed.
  ASSERT_TRUE(eng->put("k1", "v1"));
  ASSERT_TRUE(eng->put("fill1", big));

  // Second rotation should also succeed — currently it doesn't because
  // create_exclusive on the same path returns EEXIST.
  ASSERT_TRUE(eng->put("k2", "v2"));
  auto result = eng->put("fill2", big);
  EXPECT_TRUE(result.has_value()) << "second rotation failed: status="
                                  << (result ? "ok" : frankie::core::to_cstring(result.error().code_));
}

// REGRESSION (review #6): each rotation should produce its own SST file.
TEST_F(EngineRotationTest, EachRotationProducesDistinctSstFile) {
  auto eng = frankie::engine::engine::create(make_config(/*memtable_capacity=*/512));
  ASSERT_TRUE(eng.has_value());

  const std::string big(400, 'x');
  ASSERT_TRUE(eng->put("fill1", big));
  ASSERT_TRUE(eng->put("fill2", big));

  std::size_t sst_file_count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(tmp_dir_ / "segments")) {
    if (entry.is_regular_file()) {
      ++sst_file_count;
    }
  }
  EXPECT_GE(sst_file_count, 2u) << "two rotations must produce at least two SST files in segments/";
}

// REGRESSION (review #5, #7): write() uses string_view(const char*) which
// strlen-truncates at the first null byte. After rotation the SST file
// should contain all of the data block plus a serialized index and footer.
TEST_F(EngineRotationTest, RotationWritesNonTrivialSstFile) {
  auto eng = frankie::engine::engine::create(make_config(/*memtable_capacity=*/512));
  ASSERT_TRUE(eng.has_value());

  const std::string big(400, 'x');
  ASSERT_TRUE(eng->put("k", "v"));
  ASSERT_TRUE(eng->put("fill", big));  // triggers rotation

  std::uint64_t total_bytes = 0;
  for (const auto &entry : std::filesystem::directory_iterator(tmp_dir_ / "segments")) {
    if (entry.is_regular_file()) {
      total_bytes += std::filesystem::file_size(entry.path());
    }
  }

  // The data block image starts with a 20-byte header that contains a null
  // byte inside compression_type/pad. If write() string_view-truncates at
  // the first null, total_bytes will be either 0 or a tiny prefix.
  EXPECT_GE(total_bytes, sizeof(sstable_data_block_header))
      << "SST file must be at least one header long; got " << total_bytes << " bytes";
  EXPECT_GT(total_bytes, 100u) << "SST file looks truncated — likely the string_view(const char*) bug";
}

// REGRESSION (review #7): after rotation the SST file must end with a
// footer. Today no footer is ever written; this test will fail.
TEST_F(EngineRotationTest, SstFileEndsWithFooter) {
  using frankie::storage::sstable_footer;

  auto eng = frankie::engine::engine::create(make_config(/*memtable_capacity=*/512));
  ASSERT_TRUE(eng.has_value());

  const std::string big(400, 'x');
  ASSERT_TRUE(eng->put("k", "v"));
  ASSERT_TRUE(eng->put("fill", big));

  std::filesystem::path sst_path;
  for (const auto &entry : std::filesystem::directory_iterator(tmp_dir_ / "segments")) {
    if (entry.is_regular_file()) {
      sst_path = entry.path();
      break;
    }
  }
  ASSERT_FALSE(sst_path.empty()) << "no SST file produced";

  const std::uint64_t size = std::filesystem::file_size(sst_path);
  ASSERT_GE(size, sizeof(sstable_footer)) << "SST file too small to contain a footer";

  auto file = frankie::core::random_access_file::open_read(sst_path);
  ASSERT_TRUE(file.has_value());

  sstable_footer footer{};
  std::array<char, sizeof(sstable_footer)> raw{};
  auto read = file->read(std::span<char>{raw.data(), raw.size()}, size - sizeof(sstable_footer));
  ASSERT_TRUE(read.has_value());
  std::memcpy(&footer, raw.data(), sizeof(footer));

  EXPECT_GT(footer.index_offset_, 0u) << "footer must point at a serialized index region";
  EXPECT_GT(footer.index_size_, 0u) << "footer must report a non-zero index size";
  EXPECT_LE(footer.index_offset_ + footer.index_size_, size - sizeof(sstable_footer))
      << "index region must lie inside the file, before the footer";
}

// REGRESSION (review #7): the final, partially-filled block must be flushed
// to disk before WAL truncation; otherwise its entries are lost on reopen.
TEST_F(EngineRotationTest, FinalPartialBlockIsNotLost) {
  // First lifetime: insert enough to trigger rotation, then close.
  {
    auto eng = frankie::engine::engine::create(make_config(/*memtable_capacity=*/512));
    ASSERT_TRUE(eng.has_value());

    const std::string big(400, 'x');
    // The block-completion threshold (target_block_size_ = 4096) is much
    // larger than the memtable capacity (512), so the immutable memtable
    // contents fit in a single partial block that the writer never marks as
    // "complete". The engine must still flush it.
    ASSERT_TRUE(eng->put("survivor", "must-not-vanish"));
    ASSERT_TRUE(eng->put("fill", big));  // triggers rotation
  }

  // Second lifetime: WAL has been truncated, so "survivor" can only come
  // back if it was written to the SST file.
  auto eng = frankie::engine::engine::create(make_config(/*memtable_capacity=*/512));
  ASSERT_TRUE(eng.has_value());

  auto result = eng->get("survivor");
  ASSERT_TRUE(result.has_value()) << "entry was in the partial block at rotation time and got dropped";
  EXPECT_EQ(result.value(), "must-not-vanish");
}

// REGRESSION (review #9): rotation must clear memtable_immutable_ so the
// next rotation has somewhere to put its immutable. Three rotations in a
// row should all succeed.
TEST_F(EngineRotationTest, ThreeRotationsInARow) {
  auto eng = frankie::engine::engine::create(make_config(/*memtable_capacity=*/512));
  ASSERT_TRUE(eng.has_value());

  const std::string big(400, 'x');
  for (int i = 0; i < 3; ++i) {
    auto put_result = eng->put("fill" + std::to_string(i), big);
    ASSERT_TRUE(put_result.has_value()) << "rotation #" << i << " failed: "
                                        << frankie::core::to_cstring(put_result.error().code_);
  }
}

}  // namespace
