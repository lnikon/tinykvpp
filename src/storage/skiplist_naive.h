#pragma once

#include <expected>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace frankie::storage::naive {

enum class error { not_found };

// Simple node: owns key and value as std::string, forward pointers in a
// std::vector. No arena, no flat layout, no cache tricks.
struct node {
  std::string key;
  std::string value;
  std::vector<node*> next;

  node(std::string_view k, std::string_view v, int height)
      : key(k), value(v), next(static_cast<std::size_t>(height), nullptr) {}
};

class skiplist {
 public:
  static constexpr int kMaxHeight = 12;
  static constexpr int kBranchingFactor = 4;

  skiplist() : head_(std::make_unique<node>("", "", kMaxHeight)) {}

  ~skiplist() {
    node* cur = head_->next[0];
    while (cur) {
      node* nxt = cur->next[0];
      delete cur;
      cur = nxt;
    }
  }

  skiplist(const skiplist&) = delete;
  skiplist& operator=(const skiplist&) = delete;
  skiplist(skiplist&&) = delete;
  skiplist& operator=(skiplist&&) = delete;

  void insert(std::string_view key, std::string_view value) {
    node* update[kMaxHeight];
    node* cur = head_.get();

    for (int lvl = current_height_ - 1; lvl >= 0; --lvl) {
      while (cur->next[static_cast<std::size_t>(lvl)] &&
             cur->next[static_cast<std::size_t>(lvl)]->key < key) {
        cur = cur->next[static_cast<std::size_t>(lvl)];
      }
      update[lvl] = cur;
    }

    node* existing = cur->next[0];
    if (existing && existing->key == key) {
      existing->value = value;
      return;
    }

    int height = random_height();
    if (height > current_height_) {
      for (int i = current_height_; i < height; ++i) update[i] = head_.get();
      current_height_ = height;
    }

    node* n = new node(key, value, height);
    for (int i = 0; i < height; ++i) {
      n->next[static_cast<std::size_t>(i)] = update[i]->next[static_cast<std::size_t>(i)];
      update[i]->next[static_cast<std::size_t>(i)] = n;
    }
    ++count_;
  }

  [[nodiscard]] std::expected<std::string_view, error> get(std::string_view key) const noexcept {
    const node* cur = head_.get();

    for (int lvl = current_height_ - 1; lvl >= 0; --lvl) {
      while (cur->next[static_cast<std::size_t>(lvl)] &&
             cur->next[static_cast<std::size_t>(lvl)]->key < key) {
        cur = cur->next[static_cast<std::size_t>(lvl)];
      }
    }

    const node* candidate = cur->next[0];
    if (candidate && candidate->key == key) return std::string_view{candidate->value};
    return std::unexpected(error::not_found);
  }

  [[nodiscard]] std::size_t size() const noexcept { return count_; }

 private:
  int random_height() {
    int h = 1;
    while (h < kMaxHeight && (dist_(rng_) % kBranchingFactor) == 0) ++h;
    return h;
  }

  std::unique_ptr<node> head_;
  int current_height_ = 1;
  std::size_t count_ = 0;
  mutable std::mt19937 rng_{std::random_device{}()};
  std::uniform_int_distribution<int> dist_{0, INT32_MAX};
};

}  // namespace frankie::storage::naive
