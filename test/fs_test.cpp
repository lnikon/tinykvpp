#include "core/fs.hpp"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

using namespace frankie::core;

namespace {

// Slurp `path` into a vector via a fresh O_RDONLY fd. Used to verify on-disk
// state independently of the class under test.
std::vector<char> read_file(const std::filesystem::path &path) {
  std::vector<char> out;
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    return out;
  }
  std::array<char, 4096> buf{};
  ssize_t n = 0;
  while ((n = ::read(fd, buf.data(), buf.size())) > 0) {
    out.insert(out.end(), buf.data(), buf.data() + n);
  }
  ::close(fd);
  return out;
}

}  // namespace

// =============================================================================
// random_access_file
// =============================================================================

class RandomAccessFileTest : public ::testing::Test {
 protected:
  std::filesystem::path tmp_dir_;

  void SetUp() override {
    tmp_dir_ = std::filesystem::temp_directory_path() / "frankie_fs_test_raf";
    std::filesystem::create_directories(tmp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(tmp_dir_); }
};

TEST_F(RandomAccessFileTest, CreateExclusiveCreatesFile) {
  auto path = tmp_dir_ / "create_excl.bin";
  auto f = random_access_file::create_exclusive(path);
  ASSERT_TRUE(f.has_value());
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_EQ(f->path(), path);
}

TEST_F(RandomAccessFileTest, CreateExclusiveFailsIfExists) {
  auto path = tmp_dir_ / "exists.bin";
  {
    auto f = random_access_file::create_exclusive(path);
    ASSERT_TRUE(f.has_value());
  }
  auto f2 = random_access_file::create_exclusive(path);
  EXPECT_FALSE(f2.has_value());
}

TEST_F(RandomAccessFileTest, OpenReadFailsIfMissing) {
  auto f = random_access_file::open_read(tmp_dir_ / "nope.bin");
  EXPECT_FALSE(f.has_value());
}

TEST_F(RandomAccessFileTest, WriteAtOffsetThenReadBack) {
  auto path = tmp_dir_ / "rw.bin";
  auto f = random_access_file::create_exclusive(path);
  ASSERT_TRUE(f.has_value());

  std::string_view payload{"hello world"};
  auto written = f->write(payload, 0);
  ASSERT_TRUE(written.has_value());
  EXPECT_EQ(written.value(), payload.size());

  std::array<char, 11> buf{};
  auto bytes_read = f->read(std::span<char>{buf.data(), buf.size()}, 0);
  ASSERT_TRUE(bytes_read.has_value());
  EXPECT_EQ(bytes_read.value(), payload.size());
  EXPECT_EQ(std::string_view(buf.data(), payload.size()), payload);
}

TEST_F(RandomAccessFileTest, WriteAtNonZeroOffsetCreatesSparseHole) {
  auto path = tmp_dir_ / "sparse.bin";
  auto f = random_access_file::create_exclusive(path);
  ASSERT_TRUE(f.has_value());

  std::string_view payload{"tail"};
  ASSERT_TRUE(f->write(payload, 100).has_value());

  // File size advances to offset+len. Whether the hole is sparse depends on the
  // filesystem; only the size is guaranteed.
  auto sz = f->size();
  ASSERT_TRUE(sz.has_value());
  EXPECT_EQ(sz.value(), 100u + payload.size());

  std::array<char, 4> tail{};
  auto bytes_read = f->read(std::span<char>{tail.data(), tail.size()}, 100);
  ASSERT_TRUE(bytes_read.has_value());
  EXPECT_EQ(bytes_read.value(), payload.size());
  EXPECT_EQ(std::string_view(tail.data(), payload.size()), payload);
}

TEST_F(RandomAccessFileTest, OverwriteAtOffset) {
  auto path = tmp_dir_ / "overwrite.bin";
  auto f = random_access_file::create_exclusive(path);
  ASSERT_TRUE(f.has_value());

  ASSERT_TRUE(f->write("AAAAAAAA", 0).has_value());
  ASSERT_TRUE(f->write("BB", 2).has_value());

  std::array<char, 8> buf{};
  ASSERT_TRUE(f->read(std::span<char>{buf.data(), buf.size()}, 0).has_value());
  EXPECT_EQ(std::string_view(buf.data(), 8), "AABBAAAA");
}

TEST_F(RandomAccessFileTest, ReadAtEofReturnsEofStatus) {
  auto path = tmp_dir_ / "eof.bin";
  auto f = random_access_file::create_exclusive(path);
  ASSERT_TRUE(f.has_value());

  std::array<char, 4> buf{};
  auto r = f->read(std::span<char>{buf.data(), buf.size()}, 0);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(r.error().code_, status_code::eof);
}

TEST_F(RandomAccessFileTest, WriteFsyncWritesAllBytes) {
  auto path = tmp_dir_ / "wfsync.bin";
  auto f = random_access_file::create_exclusive(path);
  ASSERT_TRUE(f.has_value());

  std::string payload(8192, 'x');
  auto w = f->write_fsync(payload, 0);
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(w.value(), payload.size());

  EXPECT_EQ(read_file(path).size(), payload.size());
}

TEST_F(RandomAccessFileTest, TruncateZerosFile) {
  auto path = tmp_dir_ / "trunc.bin";
  auto f = random_access_file::create_exclusive(path);
  ASSERT_TRUE(f.has_value());

  ASSERT_TRUE(f->write("payload", 0).has_value());
  ASSERT_GT(std::filesystem::file_size(path), 0u);

  ASSERT_TRUE(f->truncate().has_value());
  EXPECT_EQ(std::filesystem::file_size(path), 0u);

  // After truncate, write at offset 0 still works.
  ASSERT_TRUE(f->write("again", 0).has_value());
  EXPECT_EQ(std::filesystem::file_size(path), 5u);
}

TEST_F(RandomAccessFileTest, MoveConstructorTransfersFd) {
  auto path = tmp_dir_ / "move_ctor.bin";
  auto f = random_access_file::create_exclusive(path);
  ASSERT_TRUE(f.has_value());
  ASSERT_TRUE(f->write("data", 0).has_value());

  random_access_file moved{std::move(*f)};
  EXPECT_EQ(moved.path(), path);
  ASSERT_TRUE(moved.write("more", 4).has_value());

  std::array<char, 8> buf{};
  ASSERT_TRUE(moved.read(std::span<char>{buf.data(), buf.size()}, 0).has_value());
  EXPECT_EQ(std::string_view(buf.data(), 8), "datamore");
}

TEST_F(RandomAccessFileTest, MoveAssignmentClosesOldFd) {
  auto path1 = tmp_dir_ / "ma1.bin";
  auto path2 = tmp_dir_ / "ma2.bin";

  auto a = random_access_file::create_exclusive(path1);
  auto b = random_access_file::create_exclusive(path2);
  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(b.has_value());

  ASSERT_TRUE(a->write("AAA", 0).has_value());
  ASSERT_TRUE(b->write("BBB", 0).has_value());

  // Steals b's fd, drops a's.
  *a = std::move(*b);
  EXPECT_EQ(a->path(), path2);

  std::array<char, 3> buf{};
  ASSERT_TRUE(a->read(std::span<char>{buf.data(), buf.size()}, 0).has_value());
  EXPECT_EQ(std::string_view(buf.data(), 3), "BBB");
}

TEST_F(RandomAccessFileTest, SizeReportsCurrentSize) {
  auto path = tmp_dir_ / "size.bin";
  auto f = random_access_file::create_exclusive(path);
  ASSERT_TRUE(f.has_value());

  EXPECT_EQ(f->size().value(), 0u);
  ASSERT_TRUE(f->write("abcdef", 0).has_value());
  EXPECT_EQ(f->size().value(), 6u);
}

TEST_F(RandomAccessFileTest, CreateOrTruncateClearsExistingContent) {
  auto path = tmp_dir_ / "cot.bin";
  {
    auto f = random_access_file::create_exclusive(path);
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(f->write("oldoldold", 0).has_value());
  }
  auto f = random_access_file::create_or_truncate(path);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(f->size().value(), 0u);
}

// =============================================================================
// append_only_file
// =============================================================================

class AppendOnlyFileTest : public ::testing::Test {
 protected:
  std::filesystem::path tmp_dir_;

  void SetUp() override {
    tmp_dir_ = std::filesystem::temp_directory_path() / "frankie_fs_test_aof";
    std::filesystem::create_directories(tmp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(tmp_dir_); }
};

TEST_F(AppendOnlyFileTest, OpenCreatesFile) {
  auto path = tmp_dir_ / "create.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_EQ(f->path(), path);
  EXPECT_EQ(f->size().value(), 0u);
}

TEST_F(AppendOnlyFileTest, OpenInvalidParentPathFails) {
  auto f = append_only_file::open(tmp_dir_ / "missing" / "deep" / "x.log");
  EXPECT_FALSE(f.has_value());
}

TEST_F(AppendOnlyFileTest, AppendWritesAtEof) {
  auto path = tmp_dir_ / "append.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());

  std::string_view a{"first;"};
  std::string_view b{"second;"};
  std::string_view c{"third;"};

  ASSERT_TRUE(f->append(a).has_value());
  ASSERT_TRUE(f->append(b).has_value());
  ASSERT_TRUE(f->append(c).has_value());

  const auto contents = read_file(path);
  EXPECT_EQ(std::string(contents.begin(), contents.end()), "first;second;third;");
}

TEST_F(AppendOnlyFileTest, AppendReturnsByteCount) {
  auto path = tmp_dir_ / "count.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());

  std::string payload(8192, 'q');
  auto w = f->append(payload);
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(w.value(), payload.size());
  EXPECT_EQ(f->size().value(), payload.size());
}

TEST_F(AppendOnlyFileTest, AppendIsAlwaysAppendNotPositional) {
  // The whole point of the type: there's no offset to mis-pass. We can only
  // verify the on-disk effect — sequential appends grow the file, never
  // overwrite.
  auto path = tmp_dir_ / "always_append.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());

  ASSERT_TRUE(f->append("AAAA").has_value());
  EXPECT_EQ(f->size().value(), 4u);

  ASSERT_TRUE(f->append("BBBB").has_value());
  EXPECT_EQ(f->size().value(), 8u);

  // Reopen and append more — still goes to EOF.
  ASSERT_TRUE(f->close().has_value());
  auto f2 = append_only_file::open(path);
  ASSERT_TRUE(f2.has_value());
  ASSERT_TRUE(f2->append("CCCC").has_value());

  const auto contents = read_file(path);
  EXPECT_EQ(std::string(contents.begin(), contents.end()), "AAAABBBBCCCC");
}

TEST_F(AppendOnlyFileTest, AppendEmptyIsNoop) {
  auto path = tmp_dir_ / "empty.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());

  auto w = f->append({});
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(w.value(), 0u);
  EXPECT_EQ(f->size().value(), 0u);
}

TEST_F(AppendOnlyFileTest, AppendFsyncSyncsAfterWrite) {
  auto path = tmp_dir_ / "fsync.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());

  ASSERT_TRUE(f->append_fsync("durable").has_value());
  // Cannot directly observe fsync, but the call must succeed and the bytes
  // must be visible to a fresh reader.
  EXPECT_EQ(read_file(path).size(), 7u);
}

TEST_F(AppendOnlyFileTest, TruncateResetsToEmpty) {
  auto path = tmp_dir_ / "truncate.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());

  ASSERT_TRUE(f->append("one;two;three;").has_value());
  ASSERT_GT(f->size().value(), 0u);

  ASSERT_TRUE(f->truncate().has_value());
  EXPECT_EQ(f->size().value(), 0u);
}

TEST_F(AppendOnlyFileTest, TruncateThenAppendStartsAtZero) {
  auto path = tmp_dir_ / "truncate_then.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());

  ASSERT_TRUE(f->append("before-truncate").has_value());
  ASSERT_TRUE(f->truncate().has_value());
  ASSERT_TRUE(f->append("after").has_value());

  const auto contents = read_file(path);
  EXPECT_EQ(std::string(contents.begin(), contents.end()), "after");
}

TEST_F(AppendOnlyFileTest, MoveConstructorTransfersFd) {
  auto path = tmp_dir_ / "move_ctor.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());
  ASSERT_TRUE(f->append("first;").has_value());

  append_only_file moved{std::move(*f)};
  EXPECT_EQ(moved.path(), path);
  ASSERT_TRUE(moved.append("second;").has_value());

  const auto contents = read_file(path);
  EXPECT_EQ(std::string(contents.begin(), contents.end()), "first;second;");
}

TEST_F(AppendOnlyFileTest, MoveAssignmentClosesOldFd) {
  auto p1 = tmp_dir_ / "ma1.log";
  auto p2 = tmp_dir_ / "ma2.log";

  auto a = append_only_file::open(p1);
  auto b = append_only_file::open(p2);
  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(b.has_value());

  ASSERT_TRUE(a->append("from-a").has_value());
  ASSERT_TRUE(b->append("from-b").has_value());

  *a = std::move(*b);
  EXPECT_EQ(a->path(), p2);
  ASSERT_TRUE(a->append(";more-b").has_value());

  const auto contents_a = read_file(p2);
  EXPECT_EQ(std::string(contents_a.begin(), contents_a.end()), "from-b;more-b");

  // p1's content was flushed by the implicit close on move-assignment.
  const auto contents_old = read_file(p1);
  EXPECT_EQ(std::string(contents_old.begin(), contents_old.end()), "from-a");
}

TEST_F(AppendOnlyFileTest, ReopenAppendsToExistingContent) {
  auto path = tmp_dir_ / "reopen.log";
  {
    auto f = append_only_file::open(path);
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(f->append("first-session;").has_value());
  }
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(f->size().value(), std::string_view("first-session;").size());
  ASSERT_TRUE(f->append("second-session").has_value());

  const auto contents = read_file(path);
  EXPECT_EQ(std::string(contents.begin(), contents.end()), "first-session;second-session");
}

TEST_F(AppendOnlyFileTest, CloseIsIdempotent) {
  auto path = tmp_dir_ / "close.log";
  auto f = append_only_file::open(path);
  ASSERT_TRUE(f.has_value());
  EXPECT_TRUE(f->close().has_value());
  EXPECT_TRUE(f->close().has_value());
}
