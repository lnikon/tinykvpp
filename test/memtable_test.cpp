#include <gtest/gtest.h>

#include "storage/memtable.hpp"
#include "test_common.hpp"

using namespace frankie::storage;
using namespace frankie::testing;
using namespace frankie::core;

// ---------------------------------------------------------------------------
// kv_entry tests
// ---------------------------------------------------------------------------

TEST(KvEntryTest, UserKeyReturnsKey) {
  kv_entry entry{.key_ = "mykey", .value_ = "myvalue", .sequence_ = 1, .timestamp_ = 1, .tombstone_ = false};
  EXPECT_EQ(entry.user_key(), "mykey");
}

TEST(KvEntryTest, ValueReturnsValue) {
  kv_entry entry{.key_ = "mykey", .value_ = "myvalue", .sequence_ = 1, .timestamp_ = 1, .tombstone_ = false};
  EXPECT_EQ(entry.value(), "myvalue");
}

TEST(InternalKeyTest, EncodedHasCorrectSize) {
  scratch_arena arena;
  internal_key ik{.user_key = "hello", .sequence = 42, .timestamp = 42, .tombstone = false};

  auto encoded = ik.encode(arena);
  // key(5) + sequence(8) + timestamp(8) + tombstone(1) = 22
  EXPECT_EQ(encoded.size(), 5 + 8 + 8 + 1);
}

TEST(InternalKeyTest, EncodedStartsWithUserKey) {
  scratch_arena arena;
  internal_key ik{.user_key = "hello", .sequence = 1, .timestamp = 1, .tombstone = false};

  auto encoded = ik.encode(arena);
  EXPECT_EQ(encoded.substr(0, 5), "hello");
}

TEST(InternalKeyTest, RoundTripPreservesSequence) {
  scratch_arena arena;
  std::uint64_t seq = 12345;
  internal_key ik{.user_key = "k", .sequence = seq, .timestamp = seq, .tombstone = false};

  auto encoded = ik.encode(arena);
  auto decoded = internal_key::decode(encoded);
  EXPECT_EQ(decoded.sequence, seq);
}

TEST(InternalKeyTest, RoundTripPreservesTimestamp) {
  scratch_arena arena;
  internal_key ik{.user_key = "k", .sequence = 1, .timestamp = 99999, .tombstone = false};

  auto encoded = ik.encode(arena);
  auto decoded = internal_key::decode(encoded);
  EXPECT_EQ(decoded.timestamp, 99999);
}

TEST(InternalKeyTest, RoundTripPreservesUserKey) {
  scratch_arena arena;
  internal_key ik{.user_key = "hello", .sequence = 1, .timestamp = 1, .tombstone = false};

  auto encoded = ik.encode(arena);
  auto decoded = internal_key::decode(encoded);
  EXPECT_EQ(decoded.user_key, "hello");
}

TEST(InternalKeyTest, EncodesTombstoneAlive) {
  scratch_arena arena;
  internal_key ik{.user_key = "k", .sequence = 1, .timestamp = 1, .tombstone = false};
  auto encoded = ik.encode(arena);
  EXPECT_EQ(encoded.back(), '\0');
}

TEST(InternalKeyTest, EncodesTombstoneDead) {
  scratch_arena arena;
  internal_key ik{.user_key = "k", .sequence = 1, .timestamp = 1, .tombstone = true};
  auto encoded = ik.encode(arena);
  EXPECT_EQ(encoded.back(), '\1');
}

TEST(InternalKeyTest, RoundTripPreservesTombstone) {
  scratch_arena arena;
  internal_key ik{.user_key = "k", .sequence = 1, .timestamp = 1, .tombstone = true};

  auto encoded = ik.encode(arena);
  auto decoded = internal_key::decode(encoded);
  EXPECT_EQ(decoded.tombstone, true);
}

TEST(KvEntryTest, BytesAllocated) {
  kv_entry entry{.key_ = "hello", .value_ = "world", .sequence_ = 1, .timestamp_ = 1, .tombstone_ = false};
  // key(5) + value(5) + seq(8) + ts(8) + tombstone(1) = 27
  EXPECT_EQ(entry.bytes_allocated(), 27);
}

// ---------------------------------------------------------------------------
// internal_key_comparator tests
// ---------------------------------------------------------------------------

TEST(InternalKeyComparatorTest, ComparesOnlyUserKeyPortion) {
  scratch_arena arena1, arena2;
  internal_key_comparator cmp;

  internal_key ik1{.user_key = "aaa", .sequence = 1, .timestamp = 1, .tombstone = false};
  internal_key ik2{.user_key = "bbb", .sequence = 2, .timestamp = 2, .tombstone = false};

  auto enc1 = ik1.encode(arena1);
  auto enc2 = ik2.encode(arena2);

  EXPECT_LT(cmp(enc1, enc2), 0);
  EXPECT_GT(cmp(enc2, enc1), 0);
}

TEST(InternalKeyComparatorTest, EqualUserKeysCompareEqual) {
  scratch_arena arena1, arena2;
  internal_key_comparator cmp;

  internal_key ik1{.user_key = "same", .sequence = 1, .timestamp = 1, .tombstone = false};
  internal_key ik2{.user_key = "same", .sequence = 99, .timestamp = 99, .tombstone = true};

  auto enc1 = ik1.encode(arena1);
  auto enc2 = ik2.encode(arena2);

  EXPECT_EQ(cmp(enc1, enc2), 0);
}

// ---------------------------------------------------------------------------
// memtable creation and basic operations
// ---------------------------------------------------------------------------

TEST(MemtableTest, CreateReturnsEmptyTable) {
  auto mt = memtable::create(1024 * 1024);
  EXPECT_EQ(mt.count(), 0);
}

TEST(MemtableTest, PutSingleEntry) {
  auto mt = memtable::create(1024 * 1024);

  mt.put("key1", "value1", 1, false);

  EXPECT_EQ(mt.count(), 1);
}

TEST(MemtableTest, PutMultipleEntries) {
  auto mt = memtable::create(1024 * 1024);

  mt.put("key1", "value1", 1, false);
  mt.put("key2", "value2", 2, false);
  mt.put("key3", "value3", 3, false);

  EXPECT_EQ(mt.count(), 3);
}

TEST(MemtableTest, PutAndGetSingleEntry) {
  auto mt = memtable::create(1024 * 1024);

  mt.put("key1", "value1", 1, false);

  auto result = mt.get("key1");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), "value1");
}

TEST(MemtableTest, PutAndGetMultipleEntries) {
  auto mt = memtable::create(1024 * 1024);

  mt.put("key1", "value1", 1, false);
  mt.put("key2", "value2", 2, false);
  mt.put("key3", "value3", 3, false);

  EXPECT_EQ(mt.count(), 3);

  auto v1 = mt.get("key1");
  EXPECT_TRUE(v1.has_value());
  EXPECT_EQ(v1->value(), "value1");

  auto v2 = mt.get("key2");
  EXPECT_TRUE(v2.has_value());
  EXPECT_EQ(v2->value(), "value2");

  auto v3 = mt.get("key3");
  EXPECT_TRUE(v3.has_value());
  EXPECT_EQ(v3->value(), "value3");
}

TEST(MemtableTest, PutMultipleKeys) {
  auto mt = memtable::create(1024 * 1024);

  mt.put("a", "1", 0, false);
  mt.put("b", "2", 0, false);
  mt.put("c", "3", 0, false);

  EXPECT_EQ(mt.get("a").value().value(), "1");
  EXPECT_EQ(mt.get("b").value().value(), "2");
  EXPECT_EQ(mt.get("c").value().value(), "3");
}

TEST(MemtableTest, GetNonExistentKeyReturnsNullopt) {
  auto mt = memtable::create(1024 * 1024);

  mt.put("key1", "value1", 1, false);

  auto result = mt.get("nonexistent");
  EXPECT_FALSE(result.has_value());
}

TEST(MemtableTest, GetFromEmptyTableReturnsNullopt) {
  auto mt = memtable::create(1024 * 1024);

  auto result = mt.get("anything");
  EXPECT_FALSE(result.has_value());
}

TEST(MemtableTest, PutOverwritesExistingKey) {
  auto mt = memtable::create(1024 * 1024);

  mt.put("key1", "value1", 1, false);
  mt.put("key1", "value2", 2, false);

  // Count increments for each put (even overwrites)
  EXPECT_EQ(mt.count(), 2);

  auto result = mt.get("key1");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), "value2");
}

TEST(MemtableTest, PutTombstoneEntry) {
  auto mt = memtable::create(1024 * 1024);

  mt.put("key1", "value1", 1, false);
  mt.put("key1", "", 2, true);

  EXPECT_EQ(mt.count(), 2);

  auto result = mt.get("key1");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), "");
}

TEST(MemtableTest, IncreasingSequenceNumbers) {
  auto mt = memtable::create(1024 * 1024);

  for (std::uint64_t i = 1; i <= 100; ++i) {
    mt.put("key" + std::to_string(i), "value" + std::to_string(i), i, false);
  }

  EXPECT_EQ(mt.count(), 100);

  for (std::uint64_t i = 1; i <= 100; ++i) {
    auto result = mt.get("key" + std::to_string(i));
    EXPECT_TRUE(result.has_value()) << "Missing key" << i;
    EXPECT_EQ(result->value(), "value" + std::to_string(i));
  }
}

TEST(MemtableTest, EmptyKeyAndValue) {
  auto mt = memtable::create(1024 * 1024);

  mt.put("", "", 1, false);

  EXPECT_EQ(mt.count(), 1);

  auto result = mt.get("");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), "");
}

TEST(MemtableTest, LargeKeyAndValue) {
  auto mt = memtable::create(1024 * 1024);

  std::string large_key(512, 'k');
  std::string large_value(4096, 'v');

  mt.put(large_key, large_value, 1, false);

  auto result = mt.get(large_key);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->value(), large_value);
}

TEST(MemtableTest, ManyEntries) {
  auto mt = memtable::create(64 * 1024 * 1024);

  const std::uint64_t n = 10000;
  for (std::uint64_t i = 0; i < n; ++i) {
    auto key = std::to_string(i);
    auto value = "val_" + std::to_string(i);
    mt.put(key, value, i + 1, false);
  }

  EXPECT_EQ(mt.count(), n);

  for (std::uint64_t i = 0; i < n; ++i) {
    auto key = std::to_string(i);
    auto result = mt.get(key);
    EXPECT_TRUE(result.has_value()) << "Missing key: " << key;
    EXPECT_EQ(result->value(), "val_" + std::to_string(i));
  }
}

TEST(MemtableTest, RandomKeyValuePairs) {
  auto mt = memtable::create(64 * 1024 * 1024);

  const std::uint64_t n = 1000;
  std::vector<std::pair<std::string, std::string>> entries;

  for (std::uint64_t i = 0; i < n; ++i) {
    auto key = random_string(random_u64(1, 64)) + std::to_string(i);
    auto value = random_string(random_u64(1, 128));
    mt.put(key, value, i + 1, false);
    entries.emplace_back(key, value);
  }

  EXPECT_EQ(mt.count(), n);

  for (const auto &[key, value] : entries) {
    auto result = mt.get(key);
    EXPECT_TRUE(result.has_value()) << "Missing key: " << key;
    EXPECT_EQ(result->value(), value);
  }
}
