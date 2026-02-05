#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace frankie {

namespace storage {

constexpr const std::uint32_t DEFAULT_MAX_HEIGHT = 12;
constexpr const std::uint32_t DEFAULT_BRANCHING_FACTOR = 4;

struct skiplist_node {
  std::string key_;
  std::string value_;
  std::uint32_t height_;
  std::vector<skiplist_node*> forward_;
};

struct skiplist {
  skiplist_node* head_;

  std::uint32_t max_height_;
  std::uint32_t current_height_;
  std::uint32_t branching_factor_;
  std::uint32_t count_{0};

  std::mt19937 rng_{std::random_device{}()};
};

std::uint32_t random_height(skiplist* sl) noexcept;

skiplist_node* create_skiplist_node(std::string_view key,
                                    std::string_view value,
                                    std::uint32_t height) noexcept;

skiplist* create_skiplist(std::uint32_t max_height,
                          std::uint32_t branching_factor) noexcept;

skiplist_node* skiplist_search(skiplist* sl, const std::string& key) noexcept;

skiplist_node* skiplist_insert(skiplist* sl, std::string_view key,
                               std::string_view value) noexcept;

}  // namespace storage

}  // namespace frankie
