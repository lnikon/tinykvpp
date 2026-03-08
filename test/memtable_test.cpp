#include <gtest/gtest.h>

#include "storage/memtable.hpp"
#include "test_common.hpp"

using namespace frankie::storage;
using namespace frankie::testing;

using comparator = simd_comparator;

TEST(MemtableTest, MemtablePut) {
  memtable memtable;

  memtable.put("key1", "value1", 1, false);
  memtable.put("key2", "value2", 1, false);

  EXPECT_EQ(memtable.count(), 2);

  auto value = memtable.get("key1");
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), "value1");

  value = memtable.get("key2");
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), "value2");
}
