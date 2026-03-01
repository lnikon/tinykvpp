#pragma once

#include <cstring>
#include <expected>
#include <random>
#include <span>
#include <string_view>

#include "core/arena.hpp"
#include "core/simd.h"

namespace frankie::storage::custom_arena {

// ---------------------------------------------------------------------------
// Error type
// ---------------------------------------------------------------------------
enum class error { not_found };

// ---------------------------------------------------------------------------
// Comparator concept
// ---------------------------------------------------------------------------
template <typename C>
concept Comparator = requires(const C &cmp, std::string_view a, std::string_view b) {
  { cmp(a, b) } -> std::convertible_to<int>;
};

struct default_comparator {
  constexpr int operator()(std::string_view a, std::string_view b) const noexcept {
    auto cmp = a <=> b;
    if (cmp < 0) return -1;
    if (cmp > 0) return 1;
    return 0;
  }
};
static_assert(Comparator<default_comparator>);

struct simd_comparator {
  constexpr int operator()(std::string_view a, std::string_view b) const noexcept {
    return simd_compare_sse2(a.data(), b.data(), a.length() < b.length() ? a.length() : b.length());
  }
};
static_assert(Comparator<simd_comparator>);

// ---------------------------------------------------------------------------
// Node — flat allocation: [skiplist_node][forward ptrs][key bytes][value bytes]
// ---------------------------------------------------------------------------
class skiplist_node {
 public:
  [[nodiscard]] static skiplist_node *create(frankie::core::arena *a, std::string_view key, std::string_view value,
                                             std::uint32_t height) noexcept {
    const auto size = layout_size(height, key.size(), value.size());
    void *mem = frankie::core::arena_allocate(a, static_cast<std::uint64_t>(size), alignof(skiplist_node));
    std::memset(mem, 0, size);

    auto *node = ::new (mem) skiplist_node{};
    node->key_size_ = static_cast<std::uint32_t>(key.size());
    node->value_size_ = static_cast<std::uint32_t>(value.size());
    node->height_ = height;

    std::ranges::fill(node->forward(), nullptr);

    std::memcpy(node->key_bytes().data(), key.data(), key.size());
    std::memcpy(node->value_bytes().data(), value.data(), value.size());

    return node;
  }

  [[nodiscard]] std::span<skiplist_node *> forward() noexcept {
    auto *base = reinterpret_cast<skiplist_node **>(reinterpret_cast<std::byte *>(this) + sizeof(skiplist_node));
    return {base, height_};
  }

  [[nodiscard]] std::span<const skiplist_node *const> forward() const noexcept {
    auto *base = reinterpret_cast<const skiplist_node *const *>(reinterpret_cast<const std::byte *>(this) +
                                                                sizeof(skiplist_node));
    return {base, height_};
  }

  [[nodiscard]] std::string_view key() const noexcept {
    return {reinterpret_cast<const char *>(key_bytes().data()), key_size_};
  }

  [[nodiscard]] std::string_view value() const noexcept {
    return {reinterpret_cast<const char *>(value_bytes().data()), value_size_};
  }

  [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

 private:
  std::uint32_t key_size_ = 0;
  std::uint32_t value_size_ = 0;
  std::uint32_t height_ = 0;

  [[nodiscard]] static constexpr std::size_t layout_size(std::uint32_t h, std::size_t ksz, std::size_t vsz) noexcept {
    return sizeof(skiplist_node) + h * sizeof(skiplist_node *) + ksz + vsz;
  }

  [[nodiscard]] std::span<std::byte> key_bytes() noexcept {
    auto *base = reinterpret_cast<std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *);
    return {base, key_size_};
  }

  [[nodiscard]] std::span<const std::byte> key_bytes() const noexcept {
    auto *base = reinterpret_cast<const std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *);
    return {base, key_size_};
  }

  [[nodiscard]] std::span<std::byte> value_bytes() noexcept {
    auto *base =
        reinterpret_cast<std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *) + key_size_;
    return {base, value_size_};
  }

  [[nodiscard]] std::span<const std::byte> value_bytes() const noexcept {
    auto *base = reinterpret_cast<const std::byte *>(this) + sizeof(skiplist_node) + height_ * sizeof(skiplist_node *) +
                 key_size_;
    return {base, value_size_};
  }
};

// ---------------------------------------------------------------------------
// Skiplist — backed by frankie::core::arena (malloc/free, bump allocator)
// ---------------------------------------------------------------------------
template <Comparator Cmp = default_comparator>
class skiplist {
 public:
  static constexpr std::uint32_t kMaxHeight = 12;
  static constexpr std::uint32_t kBranchingFactor = 4;

  explicit skiplist(Cmp cmp = {}) noexcept : cmp_{cmp} { head_ = skiplist_node::create(&arena_, {}, {}, kMaxHeight); }

  ~skiplist() { frankie::core::arena_destroy(&arena_); }

  skiplist(const skiplist &) = delete;
  skiplist &operator=(const skiplist &) = delete;
  skiplist(skiplist &&) = delete;
  skiplist &operator=(skiplist &&) = delete;

  // --- Mutations ---

  void insert(std::string_view key, std::string_view value) noexcept {
    skiplist_node *update[kMaxHeight];
    auto *current = head_;

    for (int level = static_cast<int>(current_height_) - 1; level >= 0; --level) {
      auto lvl = static_cast<std::size_t>(level);
      while (current->forward()[lvl] && cmp_(current->forward()[lvl]->key(), key) < 0) {
        current = current->forward()[lvl];
      }
      update[level] = current;
    }

    auto *existing = current->forward()[0];
    if (existing && cmp_(existing->key(), key) == 0) {
      auto h = existing->height();
      auto *replacement = skiplist_node::create(&arena_, key, value, h);
      for (std::uint32_t i = 0; i < h; ++i) {
        replacement->forward()[i] = existing->forward()[i];
        update[i]->forward()[i] = replacement;
      }
      return;
    }

    auto height = random_height();
    if (height > current_height_) {
      for (auto i = current_height_; i < height; ++i) update[i] = head_;
      current_height_ = height;
    }

    auto *node = skiplist_node::create(&arena_, key, value, height);
    for (std::uint32_t i = 0; i < height; ++i) {
      node->forward()[i] = update[i]->forward()[i];
      update[i]->forward()[i] = node;
    }
    ++count_;
  }

  // --- Lookups ---

  [[nodiscard]] std::expected<std::string_view, error> get(std::string_view key) const noexcept {
    const auto *current = head_;

    for (int level = static_cast<int>(current_height_) - 1; level >= 0; --level) {
      auto lvl = static_cast<std::size_t>(level);
      while (current->forward()[lvl] && cmp_(current->forward()[lvl]->key(), key) < 0) {
        current = current->forward()[lvl];
      }
    }

    const auto *candidate = current->forward()[0];
    if (candidate && cmp_(candidate->key(), key) == 0) return candidate->value();

    return std::unexpected(error::not_found);
  }

  // --- Capacity ---

  [[nodiscard]] std::size_t size() const noexcept { return count_; }
  [[nodiscard]] std::uint64_t bytes_allocated() const noexcept { return arena_.bytes_allocated_; }

  // --- Range-for support via sentinel iterator ---

  class iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<std::string_view, std::string_view>;

    iterator() = default;
    explicit iterator(skiplist_node *n) : node_{n} {}

    value_type operator*() const { return {node_->key(), node_->value()}; }

    iterator &operator++() {
      node_ = node_->forward()[0];
      return *this;
    }
    iterator operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    bool operator==(std::default_sentinel_t) const { return node_ == nullptr; }

   private:
    skiplist_node *node_ = nullptr;
  };

  [[nodiscard]] iterator begin() const { return iterator{head_->forward()[0]}; }
  [[nodiscard]] std::default_sentinel_t end() const { return {}; }

 private:
  std::uint32_t random_height() noexcept {
    std::uint32_t h = 1;
    while (h < kMaxHeight && (dist_(rng_) % kBranchingFactor) == 0) ++h;
    return h;
  }

  core::arena arena_{};
  skiplist_node *head_ = nullptr;
  std::uint32_t current_height_ = 1;
  std::size_t count_ = 0;

  std::mt19937 rng_{std::random_device{}()};
  std::uniform_int_distribution<std::uint32_t> dist_{0, UINT32_MAX};
  [[no_unique_address]] Cmp cmp_;
};

}  // namespace frankie::storage::custom_arena
