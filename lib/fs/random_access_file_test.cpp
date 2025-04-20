// random_access_file_test.cpp
// GoogleTest suite for fs::random_access_file::random_access_file_t
// Compile with: g++ -std=c++20 -pthread random_access_file_test.cpp -lgtest
// -lgtest_main -luring -o raf_tests

#include <gtest/gtest.h>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "random_access_file.h"

using namespace fs;
using namespace fs::random_access_file;
namespace fsstd = std::filesystem;

// ---------------------------------------------------------------------------
// Helper utilities
// ---------------------------------------------------------------------------

static std::string make_temp_file_path()
{
  // Create a unique file name inside the system temp directory.
  std::string tmpl =
      (fsstd::temp_directory_path() / "raf_test_XXXXXX").string();
  // mkstemp creates the file and returns an open fd we immediately close.
  int fd = mkstemp(tmpl.data());
  if (fd != -1)
  {
    close(fd);
  }
  return tmpl;
}

static std::string random_string(std::size_t len)
{
  static constexpr char alphabet[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  static std::mt19937                       rng{std::random_device{}()};
  static std::uniform_int_distribution<int> dist(0, sizeof(alphabet) - 2);

  std::string out;
  out.reserve(len);
  for (std::size_t i = 0; i < len; ++i)
  {
    out.push_back(alphabet[dist(rng)]);
  }
  return out;
}

// ---------------------------------------------------------------------------
// Test fixture that owns a temporary random‑access file for every test case.
// ---------------------------------------------------------------------------
class RandomAccessFileFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    path_ = make_temp_file_path();
    auto res = builder_.build(path_, /*direct_io=*/false);
    ASSERT_TRUE(res.has_value())
        << "Failed to create random_access_file_t: " << res.error().message;
    file_ = std::move(res.value());
  }

  void TearDown() override
  {
    // Explicitly destroy before unlink to ensure fd is closed.
    file_.reset();
    fsstd::remove(path_);
  }

  random_access_file_builder_t        builder_;
  std::string                         path_;
  std::optional<random_access_file_t> file_;
};

// ---------------------------------------------------------------------------
// Builder tests
// ---------------------------------------------------------------------------
TEST(RandomAccessFileBuilderTest, EmptyPathReturnsError)
{
  random_access_file_builder_t builder;
  auto                         res = builder.build("", false);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().code, file_error_code_k::open_failed);
}

TEST(RandomAccessFileBuilderTest, OpenNonExistingDirectoryFails)
{
  random_access_file_builder_t builder;
  auto res = builder.build("/this/path/does/not/exist/raf_test", false);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().code, file_error_code_k::open_failed);
}

// ---------------------------------------------------------------------------
// Happy-path operations
// ---------------------------------------------------------------------------
TEST_F(RandomAccessFileFixture, WriteAndReadRoundTripAtZero)
{
  std::string_view payload = "Hello, world!";

  // Write
  auto wres = file_->write(payload, 0);
  ASSERT_TRUE(wres.has_value());
  EXPECT_EQ(static_cast<std::size_t>(*wres), payload.size());

  // Read
  std::vector<char> buffer(payload.size());
  auto              rres = file_->read(0, buffer.data(), buffer.size());
  ASSERT_TRUE(rres.has_value());
  EXPECT_EQ(static_cast<std::size_t>(*rres), payload.size());
  EXPECT_EQ(std::string_view(buffer.data(), buffer.size()), payload);
}

class OffsetParamTest : public RandomAccessFileFixture,
                        public ::testing::WithParamInterface<std::size_t>
{
};

INSTANTIATE_TEST_SUITE_P(Offsets,
                         OffsetParamTest,
                         ::testing::Values(0, 7, 4096, 8192));

TEST_P(OffsetParamTest, WriteReadAtVariousOffsets)
{
  const std::size_t offset = GetParam();
  const std::string payload = random_string(128);

  ASSERT_TRUE(file_->write(payload, offset));

  std::vector<char> buffer(payload.size());
  auto              rres = file_->read(offset, buffer.data(), buffer.size());
  ASSERT_TRUE(rres.has_value());
  EXPECT_EQ(static_cast<std::size_t>(*rres), payload.size());
  EXPECT_EQ(std::string_view(buffer.data(), buffer.size()), payload);
}

TEST_F(RandomAccessFileFixture, SizeReflectsWrites)
{
  EXPECT_EQ(file_->size(), 0u);

  std::string payload = random_string(64);
  ASSERT_TRUE(file_->write(payload, 0));

  EXPECT_EQ(*file_->size(), payload.size());

  // Extend file by writing beyond EOF.
  ASSERT_TRUE(file_->write(payload, 1024));
  EXPECT_EQ(*file_->size(), 1024 + payload.size());
}

TEST_F(RandomAccessFileFixture, ReadBeyondEOFReturnsZero)
{
  char byte;
  auto rres = file_->read(9999, &byte, 1);
  ASSERT_TRUE(rres.has_value());
  EXPECT_EQ(*rres, 0);
}

TEST_F(RandomAccessFileFixture, FlushSucceeds)
{
  std::string payload = random_string(32);
  ASSERT_TRUE(file_->write(payload, 0));
  EXPECT_TRUE(file_->flush());
}

TEST_F(RandomAccessFileFixture, ResetTruncatesFile)
{
  ASSERT_TRUE(file_->write("data", 0));
  ASSERT_TRUE(file_->reset());
  EXPECT_EQ(file_->size(), 0u);
}

TEST_F(RandomAccessFileFixture, StreamReturnsWholeContent)
{
  std::string data1 = "LineOne\n";
  std::string data2 = "LineTwo\n";

  ASSERT_TRUE(file_->write(data1, 0));
  ASSERT_TRUE(file_->write(data2, data1.size()));

  auto sres = file_->stream();
  ASSERT_TRUE(sres.has_value());
  EXPECT_EQ(sres->str(), data1 + data2);
}

// ---------------------------------------------------------------------------
// Error‑path / negative tests
// ---------------------------------------------------------------------------
TEST(RandomAccessFileNegativeTest, OperationsOnMovedFromObjectFailGracefully)
{
  // Build a valid file first.
  auto                         path = make_temp_file_path();
  random_access_file_builder_t builder;
  auto                         res = builder.build(path, false);
  ASSERT_TRUE(res.has_value());
  random_access_file_t file = std::move(res.value());

  // Move construct to invalidate original.
  random_access_file_t moved_to = std::move(file);

  // Original object should now hold fd == -1 and ring zeroed.
  auto wres = file.write("abc", 0);
  ASSERT_FALSE(wres.has_value());
  EXPECT_EQ(wres.error().code, file_error_code_k::write_failed);

  // Clean‑up.
  std::filesystem::remove(path);
}


// ---------------------------------------------------------------------------
// Main (optional – GTest provides its own main when linked with gtest_main)
// ---------------------------------------------------------------------------
#ifndef RAF_TESTS_CUSTOM_MAIN
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
