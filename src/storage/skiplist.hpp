#pragma once

#include <cstring>
#include <expected>
#include <random>
#include <span>
#include <string_view>

#include "core/arena.hpp"
#include "core/simd.h"

namespace frankie::storage {
// ================================================================================
// Error type
// ================================================================================
enum class error { not_found };

// ================================================================================
// Comparator concept
// ================================================================================
template <typename C>
concept Comparator = requires(const C &cmp, std::string_view a, std::string_view b) {
  { cmp(a, b) } -> std::convertible_to<int>;
};

struct memcmp_comparator {
  constexpr int operator()(const std::string_view a, const std::string_view b) const noexcept {
    auto cmp = a <=> b;
    if (cmp < 0) return -1;
    if (cmp > 0) return 1;
    return 0;
  }
};
static_assert(Comparator<memcmp_comparator>);

struct simd_comparator {
  constexpr int operator()(const std::string_view a, const std::string_view b) const noexcept {
    return simd_compare_sse2(a.data(), b.data(), a.length() < b.length() ? a.length() : b.length());
  }
};
static_assert(Comparator<simd_comparator>);

using default_comparator = simd_comparator;

template <Comparator Cmp>
int compare_against_parts(const std::string_view node_key, const std::span<const std::string_view> key_parts,
                          Cmp cmp = default_comparator{}) noexcept {
  const char *lhs = node_key.data();
  std::size_t lhs_remaining = node_key.size();

  for (const auto &part : key_parts) {
    const std::size_t chunk = std::min(lhs_remaining, part.size());

    if (chunk > 0) {
      if (const int r = cmp(lhs, part.data()); r != 0) {
        return r;
      }

      lhs += chunk;
      lhs_remaining -= chunk;
    }

    // Consumed node_key, but some of the part left: A < B
    if (lhs_remaining == 0 && chunk < part.size()) {
      return -1;
    }
  }

  // Consumed all parts. If some of node_key remains then A > B, otherwise A == B
  return lhs_remaining > 0 ? 1 : 0;
}

// ================================================================================
// Node — flat allocation: [skiplist_node][forward ptrs][key bytes][value bytes]
// ================================================================================
class skiplist_node final {
 public:
  [[nodiscard]] static skiplist_node *create(core::arena *arena, std::span<const std::string_view> key_parts,
                                             std::string_view value, std::uint32_t height) noexcept;

  [[nodiscard]] std::span<skiplist_node *> forward() noexcept;

  [[nodiscard]] std::span<const skiplist_node *const> forward() const noexcept;

  [[nodiscard]] std::string_view key() const noexcept;

  [[nodiscard]] std::string_view value() const noexcept;

  [[nodiscard]] std::uint32_t height() const noexcept;

 private:
  std::uint32_t key_size_ = 0;
  std::uint32_t value_size_ = 0;
  std::uint32_t height_ = 0;

  [[nodiscard]] static constexpr std::size_t layout_size(std::uint32_t h, std::size_t ksz, std::size_t vsz) noexcept;

  [[nodiscard]] std::span<std::byte> key_bytes() noexcept;

  [[nodiscard]] std::span<const std::byte> key_bytes() const noexcept;

  [[nodiscard]] std::span<std::byte> value_bytes() noexcept;

  [[nodiscard]] std::span<const std::byte> value_bytes() const noexcept;
};

constexpr std::size_t skiplist_node::layout_size(const std::uint32_t h, const std::size_t ksz,
                                                 const std::size_t vsz) noexcept {
  return sizeof(skiplist_node) + h * sizeof(skiplist_node *) + ksz + vsz;
}

// ================================================================================
// Skiplist — backed by frankie::core::arena (malloc/free, bump allocator)
// ================================================================================
template <Comparator Cmp = default_comparator>
class skiplist final {
 public:
  static constexpr std::uint32_t kMaxHeight = 12;
  static constexpr std::uint32_t kBranchingFactor = 4;

  explicit skiplist(Cmp cmp = {}) noexcept;

  ~skiplist();

  skiplist(const skiplist &) = delete;
  skiplist &operator=(const skiplist &) = delete;
  skiplist(skiplist &&) = delete;
  skiplist &operator=(skiplist &&) = delete;

  // --- Mutations ---

  void insert(std::string_view key, std::string_view value) noexcept;
  void insert(std::span<const std::string_view> key_parts, std::string_view value) noexcept;

  // --- Lookups ---

  [[nodiscard]] std::expected<std::string_view, error> get(std::string_view key) const noexcept;

  // --- Capacity ---

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::uint64_t bytes_allocated() const noexcept;

  // --- Range-for support via sentinel iterator ---

  class iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<std::string_view, std::string_view>;

    iterator() = default;
    explicit iterator(skiplist_node *n) : node_{n} {}

    value_type operator*() const;

    iterator &operator++();
    iterator operator++(int);

    bool operator==(std::default_sentinel_t) const;

   private:
    skiplist_node *node_ = nullptr;
  };

  [[nodiscard]] iterator begin() const;
  [[nodiscard]] std::default_sentinel_t end() const;

 private:
  std::uint32_t random_height() noexcept;

  core::arena arena_{core::arena::create(core::arena::kDefaultBlockSize)};
  skiplist_node *head_ = nullptr;
  std::uint32_t current_height_ = 1;
  std::size_t count_ = 0;

  std::mt19937 rng_{std::random_device{}()};  // NOTE: Use custom rng
  std::uniform_int_distribution<std::uint32_t> dist_{0, UINT32_MAX};
  [[no_unique_address]] Cmp cmp_;
};

template <Comparator Cmp>
skiplist<Cmp>::skiplist(Cmp cmp) noexcept : cmp_{cmp} {
  head_ = skiplist_node::create(&arena_, {}, {}, kMaxHeight);
}

template <Comparator Cmp>
skiplist<Cmp>::~skiplist() {
  arena_.destroy();
}

template <Comparator Cmp>
void skiplist<Cmp>::insert(std::string_view key, std::string_view value) noexcept {
  insert(std::span{&key, 1}, value);
}

template <Comparator Cmp>
void skiplist<Cmp>::insert(const std::span<const std::string_view> key_parts, const std::string_view value) noexcept {
  skiplist_node *update[kMaxHeight];
  auto *current = head_;

  for (int level = static_cast<int>(current_height_) - 1; level >= 0; --level) {
    const auto lvl = static_cast<std::size_t>(level);
    while (current->forward()[lvl] && compare_against_parts(current->forward()[lvl]->key(), key_parts, cmp_) < 0) {
      current = current->forward()[lvl];
    }
    update[level] = current;
  }

  auto *existing = current->forward()[0];
  if (existing && compare_against_parts(existing->key(), key_parts, cmp_) == 0) {
    const auto height = existing->height();
    auto *replacement = skiplist_node::create(&arena_, key_parts, value, height);
    for (std::uint32_t i = 0; i < height; ++i) {
      replacement->forward()[i] = existing->forward()[i];
      update[i]->forward()[i] = replacement;
    }
    return;
  }

  const auto height = random_height();
  if (height > current_height_) {
    for (auto i = current_height_; i < height; ++i) update[i] = head_;
    current_height_ = height;
  }

  auto *node = skiplist_node::create(&arena_, key_parts, value, height);
  for (std::uint32_t i = 0; i < height; ++i) {
    node->forward()[i] = update[i]->forward()[i];
    update[i]->forward()[i] = node;
  }
  ++count_;
}

template <Comparator Cmp>
std::expected<std::string_view, error> skiplist<Cmp>::get(std::string_view key) const noexcept {
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

template <Comparator Cmp>
std::size_t skiplist<Cmp>::size() const noexcept {
  return count_;
}

template <Comparator Cmp>
std::uint64_t skiplist<Cmp>::bytes_allocated() const noexcept {
  return arena_.bytes_allocated();
}

template <Comparator Cmp>
skiplist<Cmp>::iterator::value_type skiplist<Cmp>::iterator::operator*() const {
  return {node_->key(), node_->value()};
}

template <Comparator Cmp>
skiplist<Cmp>::iterator &skiplist<Cmp>::iterator::operator++() {
  node_ = node_->forward()[0];
  return *this;
}

template <Comparator Cmp>
skiplist<Cmp>::iterator skiplist<Cmp>::iterator::operator++(int) {
  auto tmp = *this;
  ++*this;
  return tmp;
}

template <Comparator Cmp>
bool skiplist<Cmp>::iterator::operator==(std::default_sentinel_t) const {
  return node_ == nullptr;
}

template <Comparator Cmp>
skiplist<Cmp>::iterator skiplist<Cmp>::begin() const {
  return iterator{head_->forward()[0]};
}

template <Comparator Cmp>
std::default_sentinel_t skiplist<Cmp>::end() const {
  return {};
}

template <Comparator Cmp>
std::uint32_t skiplist<Cmp>::random_height() noexcept {
  std::uint32_t h = 1;
  while (h < kMaxHeight && (dist_(rng_) % kBranchingFactor) == 0) ++h;
  return h;
}

}  // namespace frankie::storage
