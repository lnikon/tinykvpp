#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/status.hpp"
#include "storage/segment.hpp"

namespace {

// Smoke / error-path coverage for frankie::storage::segment::create.
// The success path and segment::get_record are deliberately untested: the
// module is a partial stub (get_record has an unfinished success path).
class SegmentTest : public ::testing::Test {
 protected:
  void TearDown() override {
    if (!empty_file_path_.empty()) {
      std::error_code ec;
      std::filesystem::remove(empty_file_path_, ec);
    }
  }

  // Creates a unique zero-byte file under the system temp directory.
  // The "frankie-segment-" prefix keeps the name distinct from other parallel
  // test executables; the test name keeps it distinct within this one.
  [[nodiscard]] std::filesystem::path make_empty_file() {
    const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
    empty_file_path_ = std::filesystem::temp_directory_path() /
                       (std::string{"frankie-segment-"} + info->name() + ".sst");
    std::ofstream file{empty_file_path_};
    file.close();
    return empty_file_path_;
  }

  std::filesystem::path empty_file_path_{};
};

TEST_F(SegmentTest, CreateOnNonexistentPathReturnsNotFound) {
  const auto result = frankie::storage::segment::create("/nonexistent/path/frankie-no-such-file-xyz");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code_, frankie::core::status_code::not_found);
}

TEST_F(SegmentTest, CreateOnEmptyFileReturnsCorrupted) {
  const std::filesystem::path path = make_empty_file();
  ASSERT_TRUE(std::filesystem::exists(path));

  const auto result = frankie::storage::segment::create(path);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code_, frankie::core::status_code::corrupted);
}

}  // namespace
