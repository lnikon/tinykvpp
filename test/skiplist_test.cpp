#include "storage/skiplist.hpp"

#include <gtest/gtest.h>

#include "gtest/gtest.h"
#include "test_common.hpp"

using namespace frankie::storage;
using namespace frankie::testing;

TEST(SkiplistTest, SkiplistCreate) {
  const std::uint32_t height = 5;
  const std::uint32_t branching_factor = 5;

  skiplist* sl = create_skiplist(height, branching_factor);
  EXPECT_EQ(sl->max_height_, height);
  EXPECT_EQ(sl->current_height_, 0);
  EXPECT_EQ(sl->branching_factor_, branching_factor);
  EXPECT_EQ(sl->count_, 0);
  EXPECT_EQ(skiplist_count(sl), 0);
  EXPECT_TRUE(skiplist_empty(sl));
  EXPECT_NE(sl->head_, nullptr);
  EXPECT_EQ(sl->head_->key_, "");
  EXPECT_EQ(sl->head_->value_, "");
  EXPECT_EQ(sl->head_->height_, height);
  EXPECT_EQ(sl->head_->forward_.size(), height);
}

TEST(SkiplistTest, SkiplistInsertSingleNode) {
  const std::uint32_t height = 5;
  const std::uint32_t branching_factor = 5;
  std::string key{"hello"};
  std::string value{"world"};

  skiplist* sl = create_skiplist(height, branching_factor);

  skiplist_node* node = skiplist_insert(sl, key, value);
  EXPECT_NE(node, nullptr);
  EXPECT_EQ(node->key_, key);
  EXPECT_EQ(node->value_, value);
}

TEST(SkiplistTest, SkiplistSearch) {
  const std::uint32_t height = 5;
  const std::uint32_t branching_factor = 5;
  std::string key{"hello"};
  std::string value{"world"};

  skiplist* sl = create_skiplist(height, branching_factor);

  skiplist_node* inserted_node = skiplist_insert(sl, key, value);
  skiplist_node* found_node = skiplist_search(sl, key);
  EXPECT_NE(inserted_node, nullptr);
  EXPECT_NE(found_node, nullptr);
  EXPECT_EQ(inserted_node->key_, found_node->key_);
  EXPECT_EQ(inserted_node->value_, found_node->value_);

  EXPECT_EQ(skiplist_count(sl), 1);
  EXPECT_FALSE(skiplist_empty(sl));
}

TEST(SkiplistTest, SkiplistInsertMultipleNodes) {
  const std::uint32_t height = 10;
  const std::uint32_t branching_factor = 7;
  const std::uint32_t count = 1024;
  const std::uint64_t min_len = 1;
  const std::uint64_t max_len = 128;

  skiplist* sl = create_skiplist(height, branching_factor);

  std::vector<skiplist_node*> nodes_inserted;
  std::set<skiplist_node*> nodes_unique;
  for (std::uint64_t i = 0; i < count; i++) {
    std::string key = random_string(random_u64(min_len, max_len));
    std::string value = random_string(random_u64(min_len, max_len));

    skiplist_node* node = skiplist_insert(sl, key, value);
    EXPECT_NE(node, nullptr);
    EXPECT_EQ(node->key_, key);
    EXPECT_EQ(node->value_, value);
    nodes_inserted.push_back(node);

    nodes_unique.emplace(node);
  }

  EXPECT_EQ(skiplist_count(sl), nodes_unique.size());
  EXPECT_FALSE(skiplist_empty(sl));

  std::vector<skiplist_node*> nodes_found;
  for (std::uint64_t i = 0; i < nodes_inserted.size(); i++) {
    skiplist_node* node = skiplist_search(sl, nodes_inserted[i]->key_);
    EXPECT_NE(node, nullptr);
    nodes_found.push_back(node);
  }

  EXPECT_EQ(nodes_inserted.size(), nodes_found.size());
  for (std::uint64_t i = 0; i < nodes_found.size(); i++) {
    EXPECT_EQ(nodes_found[i]->key_.size(), nodes_inserted[i]->key_.size());
    EXPECT_EQ(nodes_found[i]->key_, nodes_inserted[i]->key_);
    EXPECT_EQ(nodes_found[i]->value_.size(), nodes_inserted[i]->value_.size());
    EXPECT_EQ(nodes_found[i]->value_, nodes_inserted[i]->value_);
  }
}

TEST(SkiplistTest, SkiplistUpdateExistingNode) {
  skiplist* sl = create_skiplist(DEFAULT_MAX_HEIGHT, DEFAULT_BRANCHING_FACTOR);
  EXPECT_NE(sl, nullptr);

  skiplist_insert(sl, "key1", "value1");
  skiplist_insert(sl, "key3", "value3");
  skiplist_insert(sl, "key2", "value2");
  skiplist_node* node = skiplist_search(sl, "key1");
  EXPECT_NE(node, nullptr);
  EXPECT_EQ(node->key_, "key1");
  EXPECT_EQ(node->value_, "value1");

  skiplist_insert(sl, "key1", "value4");
  EXPECT_NE(node, nullptr);
  EXPECT_EQ(node->key_, "key1");
  EXPECT_EQ(node->value_, "value4");
}

TEST(SkiplistTest, SkiplistSeekOnEmptyList) {
  skiplist* sl = create_skiplist(DEFAULT_MAX_HEIGHT, DEFAULT_BRANCHING_FACTOR);
  EXPECT_NE(sl, nullptr);

  skiplist_iter iter = skiplist_seek(sl, "key1");
  EXPECT_FALSE(skiplist_iter_valid(&iter));
  ASSERT_DEATH({ skiplist_iter_key(&iter); }, "\\w");
}

TEST(SkiplistTest, SkiplistSeekToNonExistingKey) {
  skiplist* sl = create_skiplist(DEFAULT_MAX_HEIGHT, DEFAULT_BRANCHING_FACTOR);
  EXPECT_NE(sl, nullptr);

  skiplist_insert(sl, "key1", "value1");
  skiplist_insert(sl, "key3", "value3");
  skiplist_insert(sl, "key2", "value2");

  skiplist_iter iter = skiplist_seek(sl, "key4");
  EXPECT_FALSE(skiplist_iter_valid(&iter));
  ASSERT_DEATH({ skiplist_iter_key(&iter); }, "\\w");
}

TEST(SkiplistTest, SkiplistSeekToExistingKey) {
  skiplist* sl = create_skiplist(DEFAULT_MAX_HEIGHT, DEFAULT_BRANCHING_FACTOR);
  EXPECT_NE(sl, nullptr);

  skiplist_insert(sl, "key1", "value1");
  skiplist_insert(sl, "key3", "value3");
  skiplist_insert(sl, "key2", "value2");

  skiplist_iter iter = skiplist_seek(sl, "key2");
  EXPECT_TRUE(skiplist_iter_valid(&iter));
  EXPECT_EQ(skiplist_iter_key(&iter), "key2");
  EXPECT_EQ(skiplist_iter_value(&iter), "value2");

  skiplist_iter_next(&iter);
  EXPECT_TRUE(skiplist_iter_valid(&iter));
  EXPECT_EQ(skiplist_iter_key(&iter), "key3");
  EXPECT_EQ(skiplist_iter_value(&iter), "value3");
}

TEST(SkiplistIteratorTest, CreateNullIterator) {
  skiplist_iter iter = create_skiplist_iter(nullptr);
  EXPECT_FALSE(skiplist_iter_valid(&iter));

  ASSERT_DEATH({ skiplist_iter_next(&iter); }, "\\w");
}

TEST(SkiplistIteratorTest, SkiplistIteratorBasicTest) {
  skiplist* sl = create_skiplist(DEFAULT_MAX_HEIGHT, DEFAULT_BRANCHING_FACTOR);
  EXPECT_NE(sl, nullptr);

  skiplist_insert(sl, "key1", "value1");
  skiplist_insert(sl, "key3", "value3");
  skiplist_insert(sl, "key2", "value2");

  skiplist_iter iter = create_skiplist_iter(sl->head_->forward_[0]);
  std::vector<std::string> values;
  while (skiplist_iter_valid(&iter)) {
    values.push_back(skiplist_iter_value(&iter));
    skiplist_iter_next(&iter);
  }

  EXPECT_EQ(skiplist_count(sl), values.size());
  EXPECT_TRUE(std::is_sorted(values.begin(), values.end()));
}
