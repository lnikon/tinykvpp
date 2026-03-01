#include "storage/skiplist_pmr.h"

#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "test_common.hpp"

using namespace frankie::storage::pmr;
using namespace frankie::testing;

TEST(SkiplistPmrTest, SkiplistCreate) {
  skiplist<> sl;
  EXPECT_EQ(sl.size(), 0);
  EXPECT_GT(sl.bytes_allocated(), 0);  // Head node is allocated
}

TEST(SkiplistPmrTest, SkiplistInsertSingleNode) {
  skiplist<> sl;

  std::string key{"hello"};
  std::string value{"world"};

  sl.insert(key, value);

  EXPECT_EQ(sl.size(), 1);

  auto result = sl.get(key);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), value);
}

TEST(SkiplistPmrTest, SkiplistSearch) {
  skiplist<> sl;

  std::string key{"hello"};
  std::string value{"world"};

  sl.insert(key, value);

  auto result = sl.get(key);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), value);

  EXPECT_EQ(sl.size(), 1);
}

TEST(SkiplistPmrTest, SkiplistSearchNotFound) {
  skiplist<> sl;

  sl.insert("key1", "value1");

  auto result = sl.get("nonexistent");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), error::not_found);
}

TEST(SkiplistPmrTest, SkiplistInsertMultipleNodes) {
  skiplist<> sl;

  const std::uint32_t count = 1024;
  const std::uint64_t min_len = 1;
  const std::uint64_t max_len = 128;

  std::vector<std::pair<std::string, std::string>> entries;
  std::set<std::string> unique_keys;

  for (std::uint64_t i = 0; i < count; i++) {
    std::string key =
        random_string(random_u64(min_len, max_len)) + std::to_string(i);
    std::string value =
        random_string(random_u64(min_len, max_len)) + std::to_string(i);

    sl.insert(key, value);
    entries.emplace_back(key, value);
    unique_keys.insert(key);
  }

  EXPECT_EQ(sl.size(), unique_keys.size());

  for (const auto& [key, value] : entries) {
    auto result = sl.get(key);
    ASSERT_TRUE(result.has_value()) << "Key not found: " << key;
    EXPECT_EQ(result.value(), value);
  }
}

TEST(SkiplistPmrTest, SkiplistUpdatesExistingValue) {
  skiplist<> sl;

  sl.insert("key1", "value1");
  sl.insert("key3", "value3");
  sl.insert("key2", "value2");

  auto result = sl.get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "value1");

  sl.insert("key1", "value4");

  result = sl.get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "value4");

  EXPECT_EQ(sl.size(), 3);
}

TEST(SkiplistPmrTest, SkiplistSearchOnEmptyList) {
  skiplist<> sl;

  auto result = sl.get("key1");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), error::not_found);
}

TEST(SkiplistPmrIteratorTest, IteratorOnEmptyList) {
  skiplist<> sl;

  auto it = sl.begin();
  EXPECT_EQ(it, sl.end());
}

TEST(SkiplistPmrIteratorTest, IteratorBasicTest) {
  skiplist<> sl;

  sl.insert("key1", "value1");
  sl.insert("key3", "value3");
  sl.insert("key2", "value2");

  std::vector<std::string> keys;
  std::vector<std::string> values;

  for (const auto& [key, value] : sl) {
    keys.emplace_back(key);
    values.emplace_back(value);
  }

  EXPECT_EQ(sl.size(), keys.size());
  EXPECT_TRUE(std::is_sorted(keys.begin(), keys.end()));
}

TEST(SkiplistPmrIteratorTest, IteratorPostIncrement) {
  skiplist<> sl;

  sl.insert("key1", "value1");
  sl.insert("key2", "value2");

  auto it = sl.begin();
  auto prev = it++;

  auto [key1, val1] = *prev;
  auto [key2, val2] = *it;

  EXPECT_EQ(key1, "key1");
  EXPECT_EQ(key2, "key2");
}

TEST(SkiplistPmrTest, CustomComparator) {
  struct reverse_comparator {
    constexpr int operator()(std::string_view a, std::string_view b) const noexcept {
      auto cmp = b <=> a;
      if (cmp < 0) return -1;
      if (cmp > 0) return 1;
      return 0;
    }
  };

  skiplist<reverse_comparator> sl;

  sl.insert("aaa", "value_a");
  sl.insert("ccc", "value_c");
  sl.insert("bbb", "value_b");

  std::vector<std::string> keys;
  for (const auto& [key, value] : sl) {
    keys.emplace_back(key);
  }

  ASSERT_EQ(keys.size(), 3);
  EXPECT_EQ(keys[0], "ccc");
  EXPECT_EQ(keys[1], "bbb");
  EXPECT_EQ(keys[2], "aaa");
}

TEST(SkiplistPmrTest, BytesAllocated) {
  skiplist<> sl;

  EXPECT_GT(sl.bytes_allocated(), 0);

  std::size_t initial = sl.bytes_allocated();

  sl.insert("key1", "value1");
  EXPECT_GT(sl.bytes_allocated(), initial);

  std::size_t after_one = sl.bytes_allocated();

  sl.insert("key2", "value2");
  EXPECT_GT(sl.bytes_allocated(), after_one);
}

TEST(SkiplistPmrTest, LargeKeysAndValues) {
  skiplist<> sl;

  std::string large_key(1024, 'k');
  std::string large_value(4096, 'v');

  sl.insert(large_key, large_value);

  auto result = sl.get(large_key);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), large_value);
}

TEST(ArenaResourceTest, BasicAllocation) {
  arena_resource arena;

  EXPECT_EQ(arena.bytes_allocated(), 0);

  void* ptr = arena.allocate(100, alignof(std::max_align_t));
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(arena.bytes_allocated(), 100);
}

TEST(ArenaResourceTest, Release) {
  arena_resource arena;

  (void)arena.allocate(100, alignof(std::max_align_t));
  (void)arena.allocate(200, alignof(std::max_align_t));

  EXPECT_EQ(arena.bytes_allocated(), 300);

  arena.release();

  EXPECT_EQ(arena.bytes_allocated(), 0);
}

TEST(ArenaResourceTest, LargeAllocation) {
  arena_resource arena(1024);

  void* ptr = arena.allocate(2048, alignof(std::max_align_t));
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(arena.bytes_allocated(), 2048);
}
