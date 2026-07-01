#include "storage/sstable_reader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <utility>

#include "core/fs.hpp"
#include "core/serialization/buffer_writer.hpp"
#include "core/views.hpp"
#include "storage/sstable_format.hpp"

using namespace frankie;

namespace {

// Encodes an 8-byte SSTable footer ([u32 index_offset][u32 index_size], little
// endian) into `buf` using the production buffer_writer.
void encode_footer(std::array<std::byte, storage::kFooterSize> &buf, std::uint32_t index_offset,
                   std::uint32_t index_size) {
  auto writer = core::buffer_writer::create(buf);
  (void)writer.write<std::uint32_t>(index_offset).write<std::uint32_t>(index_size);
}

}  // namespace

class SstableReaderTest : public ::testing::Test {
 protected:
  std::filesystem::path tmp_dir_;

  void SetUp() override {
    // Target-unique directory keeps parallel test executables from clashing.
    tmp_dir_ = std::filesystem::temp_directory_path() / "frankie_sstable_reader_test";
    std::filesystem::create_directories(tmp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(tmp_dir_); }

  // Per-test unique path under the fixture directory.
  [[nodiscard]] std::filesystem::path test_path() const {
    return tmp_dir_ / ::testing::UnitTest::GetInstance()->current_test_info()->name();
  }
};

// A zero-byte file has no footer to decode, so read_footer must report corruption.
TEST_F(SstableReaderTest, ReadFooterOnEmptyFileReturnsCorrupted) {
  const auto path = test_path();
  {
    auto file = core::random_access_file::create_or_truncate(path);
    ASSERT_TRUE(file.has_value());
  }

  auto file = core::random_access_file::open_read(path);
  ASSERT_TRUE(file.has_value());

  auto reader = storage::sstable_reader::create(std::move(*file));
  ASSERT_TRUE(reader.has_value());

  auto footer = reader->read_footer();
  ASSERT_FALSE(footer.has_value());
  EXPECT_EQ(footer.error().code_, core::status_code::corrupted);
}

// Bytes written by buffer_writer must decode back to the same footer fields.
TEST_F(SstableReaderTest, ReadFooterRoundTrip) {
  constexpr std::uint32_t kIndexOffset = 4096;
  constexpr std::uint32_t kIndexSize = 256;

  const auto path = test_path();
  std::array<std::byte, storage::kFooterSize> buf{};
  encode_footer(buf, kIndexOffset, kIndexSize);

  {
    auto file = core::random_access_file::create_or_truncate(path);
    ASSERT_TRUE(file.has_value());
    auto written = file->write(core::to_string_view(buf), 0);
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(*written, storage::kFooterSize);
  }

  auto file = core::random_access_file::open_read(path);
  ASSERT_TRUE(file.has_value());

  auto reader = storage::sstable_reader::create(std::move(*file));
  ASSERT_TRUE(reader.has_value());

  auto footer = reader->read_footer();
  ASSERT_TRUE(footer.has_value());
  EXPECT_EQ(footer->index_offset_, kIndexOffset);
  EXPECT_EQ(footer->index_size_, kIndexSize);
}

// get_data_block() is an unimplemented stub: it ignores the file and returns a
// default-constructed (empty) string_view. Pin that contract until it is built out.
TEST_F(SstableReaderTest, GetDataBlockReturnsEmptyStub) {
  const auto path = test_path();
  {
    auto file = core::random_access_file::create_or_truncate(path);
    ASSERT_TRUE(file.has_value());
  }

  auto file = core::random_access_file::open_read(path);
  ASSERT_TRUE(file.has_value());

  auto reader = storage::sstable_reader::create(std::move(*file));
  ASSERT_TRUE(reader.has_value());

  auto data_block = reader->get_data_block();
  ASSERT_TRUE(data_block.has_value());
  EXPECT_TRUE(data_block->empty());
}
