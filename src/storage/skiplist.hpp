#pragma once

#include <cstring>
#include <expected>
#include <random>
#include <span>
#include <string_view>
#include <utility>

#include "core/arena.hpp"
#include "core/simd.hpp"

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
    auto cmp = simd_compare_sse2(a.data(), b.data(), a.length() < b.length() ? a.length() : b.length());
    if (cmp != 0) {
      return cmp;
    }
    if (a.length() < b.length()) {
      return -1;
    }
    if (a.length() > b.length()) {
      return 1;
    }
    return 0;
  }
};
static_assert(Comparator<simd_comparator>);

using default_comparator = simd_comparator;

// ================================================================================
// Node — flat allocation: [skiplist_node][forward ptrs][keys bytes][values bytes]
// ================================================================================
class skiplist_node final {
 public:
  [[nodiscard]] static skiplist_node *create(core::arena *arena, std::string_view key, std::string_view value,
                                             std::uint32_t height) noexcept;

  [[nodiscard]] std::span<skiplist_node *> forward() noexcept;

  [[nodiscard]] std::span<const skiplist_node *const> forward() const noexcept;

  [[nodiscard]] std::string_view key() const noexcept;

  [[nodiscard]] std::string_view value() const noexcept;

  [[nodiscard]] std::uint32_t height() const noexcept;

  [[nodiscard]] static constexpr std::size_t layout_size(std::uint32_t h, std::size_t ksz, std::size_t vsz) noexcept;

 private:
  std::uint32_t key_size_ = 0;
  std::uint32_t value_size_ = 0;
  std::uint32_t height_ = 0;

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

  skiplist() = default;
  skiplist(const skiplist &) = delete;
  skiplist &operator=(const skiplist &) = delete;
  skiplist(skiplist &&other) noexcept;
  skiplist &operator=(skiplist &&other) noexcept;
  ~skiplist() = default;

  [[nodiscard]] static skiplist<Cmp> create(core::arena *arena, Cmp cmp) noexcept;

  // Re-point the arena backing this skiplist. Needed when the owning container
  // is moved — the arena's address changes, but the blocks it owns (and thus
  // the nodes already allocated, including head_) stay valid.
  void rebind_arena(core::arena *arena) noexcept { arena_ = arena; }

  void insert(std::string_view key, std::string_view value) noexcept;

  [[nodiscard]] std::expected<std::pair<std::string_view, std::string_view>, error> get(
      std::string_view key) const noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  [[nodiscard]] std::uint64_t bytes_allocated() const noexcept;

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

  core::arena *arena_{nullptr};
  skiplist_node *head_ = nullptr;
  std::uint32_t current_height_ = 1;
  std::size_t count_{0};
  std::size_t bytes_allocated_{0};
  std::uint64_t rng_state_{std::random_device{}() | 1};
  [[no_unique_address]] Cmp cmp_{};
};

template <Comparator Cmp>
skiplist<Cmp>::skiplist(skiplist &&other) noexcept
    : arena_{std::exchange(other.arena_, nullptr)},
      head_{std::exchange(other.head_, nullptr)},
      current_height_{std::exchange(other.current_height_, 1)},
      count_{std::exchange(other.count_, 0)},
      bytes_allocated_{std::exchange(other.bytes_allocated_, 0)},
      rng_state_{std::exchange(other.rng_state_, 0)},
      cmp_{std::exchange(other.cmp_, Cmp{})} {}

template <Comparator Cmp>
skiplist<Cmp> &skiplist<Cmp>::operator=(skiplist &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  arena_ = std::exchange(other.arena_, nullptr);
  head_ = std::exchange(other.head_, nullptr);
  current_height_ = std::exchange(other.current_height_, 1);
  count_ = std::exchange(other.count_, 0);
  bytes_allocated_ = std::exchange(other.bytes_allocated_, 0);
  rng_state_ = std::exchange(other.rng_state_, 0);
  cmp_ = std::exchange(other.cmp_, Cmp{});

  return *this;
}

template <Comparator Cmp>
skiplist<Cmp> skiplist<Cmp>::create(core::arena *arena, Cmp cmp) noexcept {
  skiplist<Cmp> result;
  result.head_ = skiplist_node::create(arena, {}, {}, kMaxHeight);
  result.bytes_allocated_ += skiplist_node::layout_size(kMaxHeight, 0, 0);
  result.arena_ = arena;
  result.cmp_ = cmp;
  return result;
}

template <Comparator Cmp>
void skiplist<Cmp>::insert(const std::string_view key, const std::string_view value) noexcept {
  std::array<skiplist_node *, kMaxHeight> update{};
  auto *current = head_;

  for (int level = static_cast<int>(current_height_) - 1; level >= 0; --level) {
    auto lvl = static_cast<std::size_t>(level);
    while (current->forward()[lvl] && cmp_(current->forward()[lvl]->key(), key) < 0) {
      current = current->forward()[lvl];
    }
    update[static_cast<std::size_t>(level)] = current;
  }

  // TODO(lnikon): Do we leak the existing node here?
  auto *existing = current->forward()[0];
  if (existing && cmp_(existing->key(), key) == 0) {
    const auto h = existing->height();
    auto *replacement = skiplist_node::create(arena_, key, value, h);
    for (std::uint32_t i = 0; i < h; ++i) {
      replacement->forward()[i] = existing->forward()[i];
      update[i]->forward()[i] = replacement;
    }

    // No need to update count_ here, as the existing node is only being replaced.
    bytes_allocated_ += skiplist_node::layout_size(h, key.size(), value.size());
    return;
  }

  const auto height = random_height();
  if (height > current_height_) {
    for (auto i = current_height_; i < height; ++i) update[i] = head_;
    current_height_ = height;
  }

  auto *const node = skiplist_node::create(arena_, key, value, height);
  for (std::uint32_t i = 0; i < height; ++i) {
    node->forward()[i] = update[i]->forward()[i];
    update[i]->forward()[i] = node;
  }

  bytes_allocated_ += skiplist_node::layout_size(height, key.size(), value.size());
  ++count_;
}

template <Comparator Cmp>
std::expected<std::pair<std::string_view, std::string_view>, error> skiplist<Cmp>::get(
    std::string_view key) const noexcept {
  const auto *current = head_;

  for (int level = static_cast<int>(current_height_) - 1; level >= 0; --level) {
    const auto lvl = static_cast<std::size_t>(level);
    while (current->forward()[lvl] && cmp_(current->forward()[lvl]->key(), key) < 0) {
      current = current->forward()[lvl];
    }
  }

  const auto *candidate = current->forward()[0];
  if (candidate && cmp_(candidate->key(), key) == 0) {
    return std::make_pair(candidate->key(), candidate->value());
  }

  return std::unexpected(error::not_found);
}

template <Comparator Cmp>
std::size_t skiplist<Cmp>::size() const noexcept {
  return count_;
}

template <Comparator Cmp>
std::uint64_t skiplist<Cmp>::bytes_allocated() const noexcept {
  return bytes_allocated_;
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
  std::uint64_t x = rng_state_;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  rng_state_ = x;

  std::uint32_t h = 1;
  while (h < kMaxHeight && (x & (kBranchingFactor - 1)) == 0) {
    ++h;
    x >>= 2;
  }
  return h;
}

}  // namespace frankie::storage
