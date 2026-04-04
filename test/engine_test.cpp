#include "engine/engine.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>

using namespace frankie::engine;

class EngineTest : public ::testing::Test {
 protected:
  std::filesystem::path tmp_dir_;

  void SetUp() override {
    tmp_dir_ = std::filesystem::temp_directory_path() / "frankie_engine_test";
    std::filesystem::create_directories(tmp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(tmp_dir_); }

  [[nodiscard]] std::filesystem::path wal_path() const { return tmp_dir_ / "test.wal"; }
};

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------

TEST_F(EngineTest, CreateSucceedsWithValidPath) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());
}

TEST_F(EngineTest, CreateFailsWithInvalidPath) {
  auto eng = engine::create("/nonexistent/dir/wal", 1024, 1024);
  ASSERT_FALSE(eng.has_value());
}

// ---------------------------------------------------------------------------
// Put / Get basics
// ---------------------------------------------------------------------------

TEST_F(EngineTest, PutAndGetSingleKey) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  ASSERT_TRUE(eng->put("key1", "value1"));

  auto result = eng->get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "value1");
}

TEST_F(EngineTest, GetNonExistentKeyReturnsNullopt) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  auto result = eng->get("missing");
  EXPECT_FALSE(result.has_value());
}

TEST_F(EngineTest, PutOverwritesValue) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  ASSERT_TRUE(eng->put("key", "v1"));
  ASSERT_TRUE(eng->put("key", "v2"));

  auto result = eng->get("key");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "v2");
}

TEST_F(EngineTest, PutMultipleKeys) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  ASSERT_TRUE(eng->put("a", "1"));
  ASSERT_TRUE(eng->put("b", "2"));
  ASSERT_TRUE(eng->put("c", "3"));

  EXPECT_EQ(eng->get("a").value(), "1");
  EXPECT_EQ(eng->get("b").value(), "2");
  EXPECT_EQ(eng->get("c").value(), "3");
}

// ---------------------------------------------------------------------------
// Delete
// ---------------------------------------------------------------------------

TEST_F(EngineTest, DeleteMakesKeyUnreachable) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  ASSERT_TRUE(eng->put("key", "value"));
  ASSERT_TRUE(eng->del("key"));

  auto result = eng->get("key");
  EXPECT_FALSE(result.has_value());
}

TEST_F(EngineTest, DeleteNonExistentKeyDoesNotCrash) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  // Tombstone for a non-existent key is a valid no-op.
  ASSERT_TRUE(eng->del("ghost"));

  EXPECT_FALSE(eng->get("ghost").has_value());
}

TEST_F(EngineTest, PutAfterDeleteRecoversKey) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  ASSERT_TRUE(eng->put("key", "v1"));
  ASSERT_TRUE(eng->del("key"));
  EXPECT_FALSE(eng->get("key").has_value());

  ASSERT_TRUE(eng->put("key", "v2"));
  auto result = eng->get("key");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "v2");
}

// ---------------------------------------------------------------------------
// Memtable rotation
// ---------------------------------------------------------------------------

TEST_F(EngineTest, RotationMovesActiveToImmutable) {
  // Use a very small memtable capacity to force rotation quickly.
  constexpr std::uint64_t kTinyCapacity = 512;
  auto eng = engine::create(wal_path(), kTinyCapacity, 4 * 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  // Insert a key that fits in the first active memtable.
  ASSERT_TRUE(eng->put("k1", "v1"));

  // Insert enough data to exceed capacity and trigger rotation.
  const std::string big_value(256, 'x');
  ASSERT_TRUE(eng->put("k2", big_value));

  // k1 should still be reachable from the immutable memtable.
  auto result = eng->get("k1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "v1");

  // k2 should be in the new active memtable.
  result = eng->get("k2");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), big_value);
}

TEST_F(EngineTest, RotationNewKeysShadowImmutable) {
  constexpr std::uint64_t kTinyCapacity = 512;
  auto eng = engine::create(wal_path(), kTinyCapacity, 4 * 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  ASSERT_TRUE(eng->put("key", "old"));

  // Force rotation.
  const std::string big(256, 'x');
  ASSERT_TRUE(eng->put("filler", big));

  // Overwrite after rotation — active memtable should shadow immutable.
  ASSERT_TRUE(eng->put("key", "new"));
  auto result = eng->get("key");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "new");
}

TEST_F(EngineTest, DeleteInImmutableIsRespected) {
  constexpr std::uint64_t kTinyCapacity = 512;
  auto eng = engine::create(wal_path(), kTinyCapacity, 4 * 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  ASSERT_TRUE(eng->put("key", "value"));
  ASSERT_TRUE(eng->del("key"));

  // Force rotation so both entries land in immutable.
  const std::string big(256, 'x');
  ASSERT_TRUE(eng->put("filler", big));

  // Tombstone must be respected even from immutable memtable.
  EXPECT_FALSE(eng->get("key").has_value());
}

// ---------------------------------------------------------------------------
// Ghost entry regression: deleted keys must not resurface after rotation
// ---------------------------------------------------------------------------

TEST_F(EngineTest, DeletedKeyInActiveDoesNotResurfaceAfterRotation) {
  constexpr std::uint64_t kTinyCapacity = 512;
  auto eng = engine::create(wal_path(), kTinyCapacity, 4 * 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  // Put and delete in the active memtable.
  ASSERT_TRUE(eng->put("ghost", "boo"));
  ASSERT_TRUE(eng->del("ghost"));
  EXPECT_FALSE(eng->get("ghost").has_value());

  // Force rotation — both put and tombstone move to immutable.
  const std::string big(256, 'x');
  ASSERT_TRUE(eng->put("filler", big));

  // The tombstone in immutable must still suppress the value.
  EXPECT_FALSE(eng->get("ghost").has_value());
}

TEST_F(EngineTest, DeleteInActiveOverridesPutInImmutable) {
  constexpr std::uint64_t kTinyCapacity = 512;
  auto eng = engine::create(wal_path(), kTinyCapacity, 4 * 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  // Put a key that will land in immutable after rotation.
  ASSERT_TRUE(eng->put("key", "alive"));

  // Force rotation — "key" is now in immutable.
  const std::string big(256, 'x');
  ASSERT_TRUE(eng->put("filler", big));

  // Delete in active memtable must shadow the immutable entry.
  ASSERT_TRUE(eng->del("key"));
  EXPECT_FALSE(eng->get("key").has_value());
}

TEST_F(EngineTest, DeleteInImmutableDoesNotBlockNewPutInActive) {
  constexpr std::uint64_t kTinyCapacity = 512;
  auto eng = engine::create(wal_path(), kTinyCapacity, 4 * 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  // Put then delete — both will move to immutable.
  ASSERT_TRUE(eng->put("key", "old"));
  ASSERT_TRUE(eng->del("key"));

  // Force rotation.
  const std::string big(256, 'x');
  ASSERT_TRUE(eng->put("filler", big));

  // Re-insert in active memtable must take precedence over immutable tombstone.
  ASSERT_TRUE(eng->put("key", "new"));
  auto result = eng->get("key");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "new");
}

TEST_F(EngineTest, MultipleDeletesDoNotResurrectKey) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  ASSERT_TRUE(eng->put("key", "value"));
  ASSERT_TRUE(eng->del("key"));
  ASSERT_TRUE(eng->del("key"));  // double delete

  EXPECT_FALSE(eng->get("key").has_value());
}

// ---------------------------------------------------------------------------
// Sequence numbers (observable via get ordering)
// ---------------------------------------------------------------------------

TEST_F(EngineTest, LatestPutWins) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(eng->put("key", std::to_string(i)));
  }

  auto result = eng->get("key");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "99");
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

TEST_F(EngineTest, MoveConstructedEngineWorks) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());
  ASSERT_TRUE(eng->put("key", "value"));

  engine moved_eng = std::move(eng.value());

  auto result = moved_eng.get("key");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "value");

  // Writes to the moved-to engine should work.
  ASSERT_TRUE(moved_eng.put("key2", "value2"));
  EXPECT_EQ(moved_eng.get("key2").value(), "value2");
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_F(EngineTest, EmptyKeyAndValue) {
  auto eng = engine::create(wal_path(), 1024 * 1024, 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  ASSERT_TRUE(eng->put("", ""));
  auto result = eng->get("");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "");
}

TEST_F(EngineTest, LargeKeyAndValue) {
  auto eng = engine::create(wal_path(), 4 * 1024 * 1024, 4 * 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  const std::string large_key(1024, 'K');
  const std::string large_value(64 * 1024, 'V');

  ASSERT_TRUE(eng->put(large_key, large_value));
  auto result = eng->get(large_key);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), large_value);
}

TEST_F(EngineTest, ManyPutsWithoutRotation) {
  auto eng = engine::create(wal_path(), 64 * 1024 * 1024, 64 * 1024 * 1024);
  ASSERT_TRUE(eng.has_value());

  constexpr int kCount = 1000;
  for (int i = 0; i < kCount; ++i) {
    ASSERT_TRUE(eng->put("k" + std::to_string(i), "v" + std::to_string(i)));
  }

  for (int i = 0; i < kCount; ++i) {
    auto result = eng->get("k" + std::to_string(i));
    ASSERT_TRUE(result.has_value()) << "missing key k" << i;
    EXPECT_EQ(result.value(), "v" + std::to_string(i));
  }
}
